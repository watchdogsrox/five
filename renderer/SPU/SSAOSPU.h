//
//	SSAO SPU header;
//
//	14/03/2007	-	Andrzej:	- initial;
//	22/12/2008	-	Andrzej:	- initial;
//	18/05/2012	-	Andrzej:	- SSAO SPU revitalized;
//
//
//
//
#ifndef __SSAOSPU_H__
#define __SSAOSPU_H__


struct SSAOSpuParams
{	
	float				m_px;
	float				m_py;
	float				m_ssaoStrength;
	u32*				m_spuProcessingTime;
};


#if __SPU

#ifndef SPU_INLINE
	#define SPU_INLINE		__forceinline
#endif

#define dumpqw(x)	Displayf(#x " = [%08x %08x %08x %08x]",si_to_uint(si_rotqbyi(x,0)),si_to_uint(si_rotqbyi(x,4)),si_to_uint(si_rotqbyi(x,8)),si_to_uint(si_rotqbyi(x,12)))
#define dumpu4(x)	Displayf(#x " = [%08x %08x %08x %08x]",si_to_uint((qword)spu_rlqwbyte(x,0)),si_to_uint((qword)spu_rlqwbyte(x,4)),si_to_uint((qword)spu_rlqwbyte(x,8)),si_to_uint((qword)spu_rlqwbyte(x,12)))
#define dumpf4(x)	Displayf(#x " = [%.4f %.4f %.4f %.4f]",si_to_float((qword)spu_rlqwbyte(x,0)),si_to_float((qword)spu_rlqwbyte(x,4)),si_to_float((qword)spu_rlqwbyte(x,8)),si_to_float((qword)spu_rlqwbyte(x,12)))

__forceinline void CheckScratchOverflow(void *scratchReg)
{
	qword t = si_xswd( (qword)si_from_ptr(scratchReg) );
	si_heqi(t,	0);				// halt if scratch has been overfilled
}

#define vecConstZero				spu_splats(0.0f)
#define vecConstOne					spu_splats(1.0f)
#define vecConstTwo					spu_splats(2.0f)
#define vecConst0_25				spu_splats(0.25f)
#define vecConstHalf				spu_splats(0.5f)
#define vecConst0_75				spu_splats(0.75f)
#define vecConst16					spu_splats(16.0f)
#define vecConst56					spu_splats(56.0f)
#define vecConst9					spu_splats(9.0f)
#define vecConst29_814				spu_splats(29.814f)
#define vecConst255					spu_splats(255.0f)

const vec_float4 vecConst0_1_2_3	= {0.0f, 1.0f, 2.0f, 3.0f};

const vec_uchar16 shufBCDa					= {0x04,0x05,0x06,0x07, 0x08,0x09,0x0A,0x0B, 0x0C,0x0D,0x0E,0x0F, 0x10,0x11,0x12,0x13};

const vec_uchar16 shufDHLP_dhlp_0000_0000	= {	0x03,0x07,0x0b,0x0f,
												0x13,0x17,0x1b,0x1f,
												0x80,0x80,0x80,0x80,
												0x80,0x80,0x80,0x80};
const vec_uchar16 shuf0000_0000_DHLP_dhlp	= {	0x80,0x80,0x80,0x80,
												0x80,0x80,0x80,0x80,
												0x03,0x07,0x0b,0x0f,
												0x13,0x17,0x1b,0x1f};


//
//     Computes the sine of each of the four slots by using a polynomia approximation
//
SPU_INLINE vec_float4 sinf4Gta(vec_float4 x)
{
	vec_float4 xl,xl2,xl3,res;
	vec_int4   q;

	// Range reduction using : xl = angle * TwoOverPi;
	//  
	xl = spu_mul(x, spu_splats(0.63661977236f));

	// Find the quadrant the angle falls in
	// using:  q = (int) (ceil(abs(xl))*sign(xl))
	//
	xl = spu_add(xl,spu_sel(spu_splats(0.5f),xl,spu_splats(0x80000000)));
	q = spu_convts(xl,0);


	// Compute an offset based on the quadrant that the angle falls in
	// 
	vec_int4 offset = spu_and(q,spu_splats((int)0x3));

	// Remainder in range [-pi/4..pi/4]
	//
	vec_float4 qf = spu_convtf(q,0);
	vec_float4 p1 = spu_nmsub(qf,spu_splats(_SINCOS_KC1),x);
	xl  = spu_nmsub(qf,spu_splats(_SINCOS_KC2),p1);

	// Compute x^2 and x^3
	//
	xl2 = spu_mul(xl,xl);
	xl3 = spu_mul(xl2,xl);


	// Compute both the sin and cos of the angles
	// using a polynomial expression:
	//   cx = 1.0f + xl2 * ((C0 * xl2 + C1) * xl2 + C2), and
	//   sx = xl + xl3 * ((S0 * xl2 + S1) * xl2 + S2)
	//
	vec_float4 ct1 = spu_madd(spu_splats(_SINCOS_CC0),xl2,spu_splats(_SINCOS_CC1));
	vec_float4 st1 = spu_madd(spu_splats(_SINCOS_SC0),xl2,spu_splats(_SINCOS_SC1));

	vec_float4 ct2 = spu_madd(ct1,xl2,spu_splats(_SINCOS_CC2));
	vec_float4 st2 = spu_madd(st1,xl2,spu_splats(_SINCOS_SC2));

	vec_float4 cx = spu_madd(ct2,xl2,spu_splats(1.0f));
	vec_float4 sx = spu_madd(st2,xl3,xl);

	// Use the cosine when the offset is odd and the sin
	// when the offset is even
	//
	vec_uchar16 mask1 = (vec_uchar16)spu_cmpeq(spu_and(offset,(int)0x1),spu_splats((int)0));
	res = spu_sel(cx,sx,mask1);

	// Flip the sign of the result when (offset mod 4) = 1 or 2
	//
	vec_uchar16 mask2 = (vec_uchar16)spu_cmpeq(spu_and(offset,(int)0x2),spu_splats((int)0));
	res = spu_sel((vec_float4)spu_xor(spu_splats(0x80000000),(vec_uint4)res),res,mask2);

	return res;
}

//
// A faster implementation of sinf4.  Returns accurate (21+ bits of the mantissa) 
// results only for inputs in the range [-pi/2 .. pi/2]. While no error is reported,
// results are unpredictable for inputs outside this range.
// sinf4fast takes about half as long to run as sinf4
//
SPU_INLINE vec_float4 sinf4fastGta(vec_float4 x)
{
    vec_float4 g = spu_mul(x,x);
    vec_float4 f = (vec_float4)spu_andc((vec_uint4)x,spu_splats(0x80000000));

    vec_float4 t1 = spu_mul(g,g);
    vec_float4 t2 = spu_madd(spu_splats(_SINCOS_R3),g,spu_splats(_SINCOS_R2));
    vec_float4 t3 = spu_mul(spu_splats(_SINCOS_R1),g);
    vec_float4 t4 = spu_mul(t1,t1);
    vec_float4 t5 = spu_madd(t1,t2,t3);
    vec_float4 r  = spu_madd(spu_splats(_SINCOS_R4),t4,t5);

    vec_float4 res = spu_madd(f,r,f);

    res = spu_sel(res, x, spu_splats(0x80000000));

    return res;
}

//
//     Computes the cosine of each of the four slots by using a polynomial approximation
//
SPU_INLINE vec_float4 cosf4Gta(vec_float4 x)
{
	vec_float4 xl,xl2,xl3,res;
	vec_int4   q;

	// Range reduction using : xl = angle * TwoOverPi;
	//  
	xl = spu_mul(x, spu_splats(0.63661977236f));

	// Find the quadrant the angle falls in
	// using:  q = (int) (ceil(abs(xl))*sign(xl))
	//
	xl = spu_add(xl,spu_sel(spu_splats(0.5f),xl,spu_splats(0x80000000)));
	q = spu_convts(xl,0);


	// Compute an offset based on the quadrant that the angle falls in
	// 
	vec_int4 offset = spu_add(spu_splats(1),spu_and(q,spu_splats((int)0x3)));

	// Remainder in range [-pi/4..pi/4]
	//
	vec_float4 qf = spu_convtf(q,0);
	vec_float4 p1 = spu_nmsub(qf,spu_splats(_SINCOS_KC1),x);
	xl  = spu_nmsub(qf,spu_splats(_SINCOS_KC2),p1);

	// Compute x^2 and x^3
	//
	xl2 = spu_mul(xl,xl);
	xl3 = spu_mul(xl2,xl);


	// Compute both the sin and cos of the angles
	// using a polynomial expression:
	//   cx = 1.0f + xl2 * ((C0 * xl2 + C1) * xl2 + C2), and
	//   sx = xl + xl3 * ((S0 * xl2 + S1) * xl2 + S2)
	//
	vec_float4 ct1 = spu_madd(spu_splats(_SINCOS_CC0),xl2,spu_splats(_SINCOS_CC1));
	vec_float4 st1 = spu_madd(spu_splats(_SINCOS_SC0),xl2,spu_splats(_SINCOS_SC1));

	vec_float4 ct2 = spu_madd(ct1,xl2,spu_splats(_SINCOS_CC2));
	vec_float4 st2 = spu_madd(st1,xl2,spu_splats(_SINCOS_SC2));

	vec_float4 cx = spu_madd(ct2,xl2,spu_splats(1.0f));
	vec_float4 sx = spu_madd(st2,xl3,xl);

	// Use the cosine when the offset is odd and the sin
	// when the offset is even
	//
	vec_uchar16 mask1 = (vec_uchar16)spu_cmpeq(spu_and(offset,(int)0x1),spu_splats((int)0));
	res = spu_sel(cx,sx,mask1);

	// Flip the sign of the result when (offset mod 4) = 1 or 2
	//
	vec_uchar16 mask2 = (vec_uchar16)spu_cmpeq(spu_and(offset,(int)0x2),spu_splats((int)0));
	res = spu_sel((vec_float4)spu_xor(spu_splats(0x80000000),(vec_uint4)res),res,mask2);

	return res;

}

//
// A faster implementation of cosf4.  Returns accurate (21+ bits of the mantissa) 
// results only for inputs in the range [0..pi].  While no error is reported, 
// results are unpredictable for inputs outside this range.
// cosf4fast takes about one third less time to run as cosf4.
//
SPU_INLINE vec_float4 cosf4fastGta(vec_float4 x)
{
    // cos(x) = sin(pi/2 - x)
    //
    x = spu_sub(spu_splats(_SINCOS_KC1),x);
    x = spu_add(spu_splats(_SINCOS_KC2),x);

    vec_float4 g = spu_mul(x,x);
    vec_float4 f = (vec_float4)spu_andc((vec_uint4)x,spu_splats(0x80000000));

    vec_float4 t1 = spu_mul(g,g);
    vec_float4 t2 = spu_madd(spu_splats(_SINCOS_R3),g,spu_splats(_SINCOS_R2));
    vec_float4 t3 = spu_mul(spu_splats(_SINCOS_R1),g);
    vec_float4 t4 = spu_mul(t1,t1);
    vec_float4 t5 = spu_madd(t1,t2,t3);
    vec_float4 r  = spu_madd(spu_splats(_SINCOS_R4),t4,t5);

    vec_float4 res = spu_madd(f,r,f);

    res = spu_sel(res, x, spu_splats(0x80000000));

     return res;
}

//
//     Computes both the sine and cosine of the all four slots of x
//     by using a polynomial approximation.
//
SPU_INLINE
void sincosf4Gta(vec_float4 x, vec_float4& s, vec_float4& c)
{
    vec_float4 xl,xl2,xl3;
    vec_int4   q;
    vec_int4   offsetSin, offsetCos;

    // Range reduction using : xl = angle * TwoOverPi;
    //  
    xl = spu_mul(x, spu_splats(0.63661977236f));

    // Find the quadrant the angle falls in
    // using:  q = (int) (ceil(abs(xl))*sign(xl))
    //
    xl = spu_add(xl,spu_sel(spu_splats(0.5f),xl,spu_splats(0x80000000)));
    q = spu_convts(xl,0);

     
    // Compute the offset based on the quadrant that the angle falls in.
    // Add 1 to the offset for the cosine. 
    //
    offsetSin = spu_and(q,spu_splats((int)0x3));
    offsetCos = spu_add(spu_splats(1),offsetSin);

    // Remainder in range [-pi/4..pi/4]
    //
    vec_float4 qf = spu_convtf(q,0);
    vec_float4 p1 = spu_nmsub(qf,spu_splats(_SINCOS_KC1),x);
    xl  = spu_nmsub(qf,spu_splats(_SINCOS_KC2),p1);
    
    // Compute x^2 and x^3
    //
    xl2 = spu_mul(xl,xl);
    xl3 = spu_mul(xl2,xl);
    

    // Compute both the sin and cos of the angles
    // using a polynomial expression:
    //   cx = 1.0f + xl2 * ((C0 * xl2 + C1) * xl2 + C2), and
    //   sx = xl + xl3 * ((S0 * xl2 + S1) * xl2 + S2)
    //
    vec_float4 ct1 = spu_madd(spu_splats(_SINCOS_CC0),xl2,spu_splats(_SINCOS_CC1));
    vec_float4 st1 = spu_madd(spu_splats(_SINCOS_SC0),xl2,spu_splats(_SINCOS_SC1));

    vec_float4 ct2 = spu_madd(ct1,xl2,spu_splats(_SINCOS_CC2));
    vec_float4 st2 = spu_madd(st1,xl2,spu_splats(_SINCOS_SC2));
    
    vec_float4 cx = spu_madd(ct2,xl2,spu_splats(1.0f));
    vec_float4 sx = spu_madd(st2,xl3,xl);

    // Use the cosine when the offset is odd and the sin
    // when the offset is even
    //
    vec_uchar16 sinMask = (vec_uchar16)spu_cmpeq(spu_and(offsetSin,(int)0x1),spu_splats((int)0));
    vec_uchar16 cosMask = (vec_uchar16)spu_cmpeq(spu_and(offsetCos,(int)0x1),spu_splats((int)0));    
    s = spu_sel(cx,sx,sinMask);
    c = spu_sel(cx,sx,cosMask);

    // Flip the sign of the result when (offset mod 4) = 1 or 2
    //
    sinMask = (vec_uchar16)spu_cmpeq(spu_and(offsetSin,(int)0x2),spu_splats((int)0));
    cosMask = (vec_uchar16)spu_cmpeq(spu_and(offsetCos,(int)0x2),spu_splats((int)0));
    
    s = spu_sel((vec_float4)spu_xor(spu_splats(0x80000000),(vec_uint4)s),s,sinMask);
    c = spu_sel((vec_float4)spu_xor(spu_splats(0x80000000),(vec_uint4)c),c,cosMask);
    
}


#define _EXP2F_H_LN2	0.69314718055995f	/* ln(2) */

SPU_INLINE vec_float4 exp2f4Gta(vec_float4 x)
{
	vec_int4 ix;
	vec_uint4 overflow, underflow;
	vec_float4 frac, frac2, frac4;
	vec_float4 exp_int, exp_frac;
	vec_float4 result;
	vec_float4 hi, lo;

	vec_float4 bias;
	/* Break in the input x into two parts ceil(x), x - ceil(x).
	*/
	bias = (vec_float4)(spu_rlmaska((vec_int4)(x), -31));
	bias = (vec_float4)(spu_andc((vec_uint4)(0x3F7FFFFF), (vec_uint4)bias));
	ix = spu_convts(spu_add(x, bias), 0);
	frac = spu_sub(spu_convtf(ix, 0), x);
	frac = spu_mul(frac, (vec_float4)(_EXP2F_H_LN2));

	// !!! HRD Changing weird un-understandable and incorrect overflow handling code
	//overflow = spu_sel((vec_uint4)spu_splats(0x7FFFFFFF), (vec_uint4)x, (vec_uchar16)spu_splats(0x80000000));  
	overflow = spu_cmpgt(x, (vec_float4)spu_splats(0x4300FFFF)); // !!! Biggest possible exponent to fit in range.
	underflow = spu_cmpgt(spu_splats(-126.0f), x);

	//exp_int = (vec_float4)(spu_sl(spu_add(ix, 127), 23)); // !!! HRD <- changing this to correct for
	// !!! overflow (x >= 127.999999f)
	exp_int = (vec_float4)(spu_sl(spu_add(ix, 126), 23));   // !!! HRD <- add with saturation
	exp_int = spu_add(exp_int, exp_int);                    // !!! HRD

	/* Instruction counts can be reduced if the polynomial was
	* computed entirely from nested (dependent) fma's. However, 
	* to reduce the number of pipeline stalls, the polygon is evaluated 
	* in two halves (hi amd lo). 
	*/
	frac2 = spu_mul(frac, frac);
	frac4 = spu_mul(frac2, frac2);

	hi = spu_madd(frac, (vec_float4)(-0.0001413161), (vec_float4)(0.0013298820));
	hi = spu_madd(frac, hi, (vec_float4)(-0.0083013598));
	hi = spu_madd(frac, hi, (vec_float4)(0.0416573475));
	lo = spu_madd(frac, (vec_float4)(-0.1666653019), (vec_float4)(0.4999999206));
	lo = spu_madd(frac, lo, (vec_float4)(-0.9999999995));
	lo = spu_madd(frac, lo, (vec_float4)(1.0));

	exp_frac = spu_madd(frac4, hi, lo);
	ix = spu_add(ix, spu_rlmask((vec_int4)(exp_frac), -23));
	result = spu_mul(exp_frac, exp_int);

	/* Handle overflow */
	result = spu_sel(result, (vec_float4)spu_splats(0x7FFFFFFF), (vec_uchar16)overflow); 
	result = spu_sel(result, (vec_float4)spu_splats(0), (vec_uchar16)underflow);
	//result = spu_sel(result, (vec_float4)(overflow), spu_cmpgt((vec_uint4)(ix), 255));

	return (result);
}

#define _LOG2F_H_l2emsb ((float)1.4426950216293f) 
#define _LOG2F_H_l2elsb ((float)1.9259629911e-8f) 
#define _LOG2F_H_l2e   ((float)1.4426950408890f) 

#define _LOG2F_H_c0 ((float)(0.2988439998f)) 
#define _LOG2F_H_c1 ((float)(0.3997655209f))
#define _LOG2F_H_c2 ((float)(0.6666679125f))

SPU_INLINE vec_float4 log2f4Gta(vec_float4 x)
{
	vec_int4 zeros = spu_splats((int)0);
	vec_float4 ones = spu_splats(1.0f);
	vec_uchar16 zeromask = (vec_uchar16)spu_cmpeq(x, (vec_float4)zeros);

	vec_int4 expmask = spu_splats((int)0x7F800000);
	vec_int4 xexp = spu_add( spu_rlmask(spu_and((vec_int4)x, expmask), -23), -126 );
	x = spu_sel(x, (vec_float4)spu_splats((int)0x3F000000), (vec_uchar16)expmask);


	vec_uint4  mask = spu_cmpgt(spu_splats((float)0.7071067811865f), x);
	x    = spu_sel(x   , spu_add(x, x)                   , mask);
	xexp = spu_sel(xexp, spu_sub(xexp,spu_splats((int)1)), mask);

	vec_float4 x1 = spu_sub(x , ones);
	vec_float4 z  = divf4(x1, spu_add(x, ones));
	vec_float4 w  = spu_mul(z , z);
	vec_float4 polyw;
	polyw = spu_madd(spu_splats(_LOG2F_H_c0), w, spu_splats(_LOG2F_H_c1));
	polyw = spu_madd(polyw                  , w, spu_splats(_LOG2F_H_c2));

	vec_float4 yneg = spu_mul(z, spu_msub(polyw, w, x1));
	vec_float4 zz1 = spu_madd(spu_splats(_LOG2F_H_l2emsb), x1, spu_convtf(xexp,0));
	vec_float4 zz2 = spu_madd(spu_splats(_LOG2F_H_l2elsb), x1,
		spu_mul(spu_splats(_LOG2F_H_l2e), yneg)
		);

	return spu_sel(spu_add(zz1,zz2), (vec_float4)zeromask, zeromask);
}

SPU_INLINE vec_float4 powf4Gta(vec_float4 x, vec_float4 y)
{
	vec_int4 zeros = spu_splats((int)0);
	vec_uchar16 zeromask = (vec_uchar16)spu_cmpeq((vec_float4)zeros, x);

	vec_uchar16 negmask  = (vec_uchar16)spu_cmpgt(spu_splats(0.0f), x);

	vec_float4 sbit = (vec_float4)spu_splats((int)0x80000000);
	vec_float4 absx = spu_andc(x, sbit);
	vec_float4 absy = spu_andc(y, sbit);
	vec_uint4 oddy = spu_and(spu_convtu(absy, 0), (vec_uint4)spu_splats(0x00000001));
	negmask = spu_and(negmask, (vec_uchar16)spu_cmpgt(oddy, (vec_uint4)zeros));

	vec_float4 res = exp2f4Gta(spu_mul(y, log2f4Gta(absx)));
	res = spu_sel(res, spu_or(sbit, res), negmask);


	return spu_sel(res, (vec_float4)zeros, zeromask);
}


#endif //__SPU...
#endif //__SSAOSPU_H__...

