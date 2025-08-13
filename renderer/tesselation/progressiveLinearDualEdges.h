#ifndef progressiveLinearDualEdgesTesselator_H
#define progressiveLinearDualEdgesTesselator_H

template<class T> class progressiveLinearDualEdgesTesselator
{
public:
	typedef Functor1<const T &> pushVtxFunctor;
	typedef Functor3<const T &,const T &,const T &> pushTriFunctor;

	void StripTesselate(const T* vertices, float level, float outerLevel, typename progressiveLinearDualEdgesTesselator<T>::pushVtxFunctor pushVtx);
	int StripGetVtxCount(float level, float outerLevel)
	{
		float tesselateLevel = Floorf(level+1);
		int iTesselate = (int)tesselateLevel;
		
		int iTesselate2 = Max((int)(outerLevel+1),iTesselate);
		
		return	iTesselate + (iTesselate) * (iTesselate + 2) + (iTesselate2 - iTesselate) * 2 - 1;
	}
	
	void ListTesselate(const T* vertices, float level, float outerLevel, typename progressiveLinearDualEdgesTesselator<T>::pushTriFunctor pushVtx);
	int ListGetVtxCount	(float level, float outerLevel)
	{
		float tesselateLevel = Floorf(level+1.0f);
		int iTesselate = (int)tesselateLevel;
		int iTesselate2 = max((int)(outerLevel+1.0f),iTesselate);

		return iTesselate * iTesselate * 3 + (iTesselate2 - iTesselate) * 3 - ( iTesselate < 3 ? 9 : 0 );
	}

};


template<class T> void progressiveLinearDualEdgesTesselator<T>::StripTesselate(const T* vertices, float l, float ol, typename progressiveLinearDualEdgesTesselator<T>::pushVtxFunctor pushVtx)
{
	float tesselateLevel = l;
	float tesselateProg = l - Floorf(l);

	float outerLevel = Max(ol,l);
	
	T v01;
	v01 = vertices[1];
	v01.sub(vertices[0]);
	v01.div(tesselateLevel);

	T v02;
	v02 = vertices[2];
	v02.sub(vertices[0]);
	v02.div(tesselateLevel);

	T v21;
	v21 = vertices[1];
	v21.sub(vertices[2]);
	v21.div(tesselateLevel);
	
	T prevVtx = vertices[0];
	T currVtx = vertices[0];
	T deltaA = v02;
	T deltaB = v21;

	T progDeltaA = deltaA;
	progDeltaA.mul(tesselateProg);

	T progDeltaB = deltaB;
	progDeltaB.mul(tesselateProg);

	T deltaC = v01;
	deltaC.mul(tesselateProg);
	deltaC.sub(deltaB);
		
	tesselateLevel = Floorf(l+1.0f);
	int iTesselate = (int)tesselateLevel;
	int iLevel = iTesselate;
	int ii=0;
	T tmp;
	
	int iTesselate2 = (int)outerLevel;
	int iLevel2 = iTesselate2;

	T deltaA2;
	T deltaB2;
	
	v01 = vertices[1];
	v01.sub(vertices[0]);
	v01.div(outerLevel);

	v02 = vertices[2];
	v02.sub(vertices[0]);
	v02.div(outerLevel);

	v21 = vertices[1];
	v21.sub(vertices[2]);
	v21.div(outerLevel);
	
	deltaA2 = v02;
	deltaB2 = v21;

	vtxDcl currentOuterVtx = currVtx;
	vtxDcl currentInnerVtx = currVtx;

	currentInnerVtx.add(deltaA);

	T innerDelta = deltaA;
	innerDelta.add(deltaB);
	
	T progInnerDelta = innerDelta;
	progInnerDelta.mul(tesselateProg);
	
	T outerDelta = deltaA2;
	outerDelta.add(deltaB2);

	for(int jj=0;jj<iLevel-2;jj++)
	{
		pushVtx(currentOuterVtx);
		currentOuterVtx.add(outerDelta);
		
		pushVtx(currentInnerVtx);
		currentInnerVtx.add(innerDelta);
	}
	iLevel--;

	pushVtx(currentOuterVtx);
	currentOuterVtx.add(outerDelta);
	
	pushVtx(currentInnerVtx);
	currentInnerVtx.add(progInnerDelta);

	for(int i=iLevel;i<iLevel2+1;i++)
	{
		pushVtx(currentOuterVtx);
		currentOuterVtx.add(outerDelta);
		
		pushVtx(currentInnerVtx);
	}

	pushVtx(vertices[1]);

	deltaA.mul(-1.0f);
	deltaB.mul(-1.0f);
	deltaC.mul(-1.0f);
	
	progDeltaA.mul(-1.0f);
	progDeltaB.mul(-1.0f);
	
	tmp = deltaA;
	deltaA = deltaB;
	deltaB = tmp;

	tmp = progDeltaA;
	progDeltaA = progDeltaB;
	progDeltaB = tmp;

	currVtx = vertices[1];
	currVtx.add(deltaA);

	pushVtx(currVtx);
	pushVtx(currVtx);

	for(ii=1;ii<iTesselate-1;ii++)
	{
		bool odd = 1 == (ii & 1);
		
		currVtx.add(deltaA);
		pushVtx(currVtx);

		if( odd )
		{
			currVtx.add(deltaC);
			pushVtx(currVtx);
		}
		else
		{
			currVtx.add(deltaB);
			pushVtx(currVtx);
		}			
		
		for(int jj=1;jj<iLevel-1;jj++)
		{
			currVtx.add(deltaA);
			pushVtx(currVtx);

			currVtx.add(deltaB);
			pushVtx(currVtx);			
		}
		iLevel--;

		if( odd )
		{
			currVtx.add(deltaA);
			pushVtx(currVtx);
		}
		else
		{
			currVtx.add(deltaC);
			pushVtx(currVtx);
		}

		currVtx.add(deltaB);
		pushVtx(currVtx);
					
		deltaA.mul(-1.0f);
		deltaB.mul(-1.0f);
		deltaC.mul(-1.0f);
		
		progDeltaA.mul(-1.0f);
		progDeltaB.mul(-1.0f);
		
		tmp = deltaA;
		deltaA = deltaB;
		deltaB = tmp;

		tmp = progDeltaA;
		progDeltaA = progDeltaB;
		progDeltaB = tmp;
		 
		currVtx.add(deltaA);
		pushVtx(currVtx);
		pushVtx(currVtx);
	}
	
	if( iTesselate > 1 )
	{
		currVtx.add(progDeltaA);
		pushVtx(currVtx);

		currVtx.add(progDeltaB);
		pushVtx(currVtx);			
	}
}

template<class T> void progressiveLinearDualEdgesTesselator<T>::ListTesselate(const T* vertices, float l, float ol, typename progressiveLinearDualEdgesTesselator<T>::pushTriFunctor pushTri)
{
	float tesselateLevel = l;
	float tesselateProg = l - Floorf(l);

	float outerLevel = max(ol,l);
	
	T v01;
	v01 = vertices[1];
	v01.sub(vertices[0]);
	v01.div(tesselateLevel);

	T v02;
	v02 = vertices[2];
	v02.sub(vertices[0]);
	v02.div(tesselateLevel);

	T v21;
	v21 = vertices[1];
	v21.sub(vertices[2]);
	v21.div(tesselateLevel);
	
	T prevVtx = vertices[0];
	T currVtx = vertices[0];
	T deltaA = v02;
	T deltaB = v21;

	T progDeltaA = deltaA;
	progDeltaA.mul(tesselateProg);

	T progDeltaB = deltaB;
	progDeltaB.mul(tesselateProg);

	T deltaC = v01;
	deltaC.mul(tesselateProg);
	deltaC.sub(deltaB);

	T p1,p2,p3;
		
	tesselateLevel = Floorf(l+1.0f);
	int iTesselate = (int)tesselateLevel;
	int iLevel = iTesselate;
	int ii=0;
	T tmp;
	
	int iTesselate2 = (int)outerLevel;
	int iLevel2 = iTesselate2;

	T deltaA2;
	T deltaB2;
	
	v01 = vertices[1];
	v01.sub(vertices[0]);
	v01.div(outerLevel);

	v02 = vertices[2];
	v02.sub(vertices[0]);
	v02.div(outerLevel);

	v21 = vertices[1];
	v21.sub(vertices[2]);
	v21.div(outerLevel);
	
	deltaA2 = v02;
	deltaB2 = v21;

	vtxDcl currentOuterVtx = currVtx;
	vtxDcl currentInnerVtx = currVtx;

	currentInnerVtx.add(deltaA);

	T innerDelta = deltaA;
	innerDelta.add(deltaB);
	
	T progInnerDelta = innerDelta;
	progInnerDelta.mul(tesselateProg);
	
	T outerDelta = deltaA2;
	outerDelta.add(deltaB2);

	p1 = p2 = p3 = currentOuterVtx;

	if( iTesselate > 2 )
	{
		p2 = currentOuterVtx;
		p3 = currentInnerVtx;

		currentOuterVtx.add(outerDelta);
		currentInnerVtx.add(innerDelta);

		for(int jj=1;jj<iLevel-2;jj++)
		{
			p1 = p2;
			p2 = p3;
			p3 = currentOuterVtx;
			pushTri(p1,p2,p3);
			currentOuterVtx.add(outerDelta);
			
			p1 = p2;
			p2 = p3;
			p3 = currentInnerVtx;
			pushTri(p1,p2,p3);
			currentInnerVtx.add(innerDelta);
		}

		p1 = p2;
		p2 = p3;
		p3 = currentOuterVtx;
		pushTri(p1,p2,p3);
		currentOuterVtx.add(outerDelta);
		
		p1 = p2;
		p2 = p3;
		p3 = currentInnerVtx;
		pushTri(p1,p2,p3);
		currentInnerVtx.add(progInnerDelta);

		p1 = p2;
		p2 = p3;
		p3 = currentOuterVtx;
		pushTri(p1,p2,p3);

		p1 = p2;
		p2 = p3;
		p3 = currentInnerVtx;
		pushTri(p1,p2,p3);	

		p1 = p3;
		p2 = currentOuterVtx;
		p3 = currentInnerVtx;
		
		currentOuterVtx.add(outerDelta);
		
	}
	else
	{
		p1 = vertices[0];
		p2 = vertices[2];

		currentOuterVtx.add(outerDelta);
		currentInnerVtx = vertices[2];
	}

	iLevel--;

	for(int i=iLevel+1;i<iLevel2+1;i++)
	{
		p1 = p2;
		p2 = p3;
		p3 = currentOuterVtx;
		pushTri(p1,p2,p3);
		currentOuterVtx.add(outerDelta);

		p1 = p2;
		p2 = p3;
		p3 = currentInnerVtx;
	}

	p1 = p2;
	p2 = p3;
	p3 = vertices[1];
	pushTri(p1,p2,p3);	

	deltaA.mul(-1.0f);
	deltaB.mul(-1.0f);
	deltaC.mul(-1.0f);
	
	progDeltaA.mul(-1.0f);
	progDeltaB.mul(-1.0f);
	
	tmp = deltaA;
	deltaA = deltaB;
	deltaB = tmp;

	tmp = progDeltaA;
	progDeltaA = progDeltaB;
	progDeltaB = tmp;

	currVtx = vertices[1];
	currVtx.add(deltaA);

	p1 = p3;
	p2 = currVtx;
	p3 = currVtx;

	for(ii=1;ii<iTesselate-1;ii++)
	{
		bool odd = 1 == (ii & 1);
		
		currVtx.add(deltaA);
		p1 = p2;
		p2 = p3;
		p3 = currVtx;

		if( odd )
		{
			currVtx.add(deltaC);
		}
		else
		{
			currVtx.add(deltaB);
		}			

		p1 = p2;
		p2 = p3;
		p3 = currVtx;
		pushTri(p1,p2,p3);
		
		for(int jj=1;jj<iLevel-1;jj++)
		{
			currVtx.add(deltaA);
			p1 = p2;
			p2 = p3;
			p3 = currVtx;
			pushTri(p1,p2,p3);

			currVtx.add(deltaB);
			p1 = p2;
			p2 = p3;
			p3 = currVtx;
			pushTri(p1,p2,p3);
		}
		iLevel--;

		if( odd )
		{
			currVtx.add(deltaA);
		}
		else
		{
			currVtx.add(deltaC);
		}

		p1 = p2;
		p2 = p3;
		p3 = currVtx;
		pushTri(p1,p2,p3);

		currVtx.add(deltaB);
		p1 = p2;
		p2 = p3;
		p3 = currVtx;
		pushTri(p1,p2,p3);
					
		deltaA.mul(-1.0f);
		deltaB.mul(-1.0f);
		deltaC.mul(-1.0f);
		
		progDeltaA.mul(-1.0f);
		progDeltaB.mul(-1.0f);
		
		tmp = deltaA;
		deltaA = deltaB;
		deltaB = tmp;

		tmp = progDeltaA;
		progDeltaA = progDeltaB;
		progDeltaB = tmp;
		 
		currVtx.add(deltaA);

		p1 = p3;
		p2 = currVtx;
		p3 = currVtx;
	}
	
	if( iTesselate > 2 )
	{
		p1 = p3;

		currVtx.add(progDeltaA);
		p2 = currVtx;

		currVtx.add(progDeltaB);
		p3 = currVtx;
		
		pushTri(p1,p2,p3);
	}
}
#endif // progressiveLinearDualEdgesTesselator_H