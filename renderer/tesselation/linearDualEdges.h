#ifndef LINEARDUALEDGESTESSELATOR_H
#define LINEARDUALEDGESTESSELATOR_H

template<class T> class linearDualEdgesTesselator
{
public:
	typedef Functor1<const T &> pushVtxFunctor;
	typedef Functor3<const T &,const T &,const T &> pushTriFunctor;

	void StripTesselate(const T* vertices, float level, float outerLevel, typename linearDualEdgesTesselator<T>::pushVtxFunctor pushVtx);
	int StripGetVtxCount(float level, float outerLevel)
	{
		float tesselateLevel = Floorf(level);
		int iTesselate = (int)tesselateLevel;
		
		int iTesselate2 = max((int)outerLevel,iTesselate);
		
		return	iTesselate + (iTesselate) * (iTesselate + 2) - 1 + 2 + 
				((iTesselate2 > iTesselate) ? (( iTesselate2 - iTesselate) * 2) : (0));
	}
	
	void ListTesselate(const T* vertices, float level, float outerLevel, typename linearDualEdgesTesselator<T>::pushTriFunctor pushVtx);
	int ListGetVtxCount	(float level, float outerLevel)
	{
		float tesselateLevel = Floorf(level);
		int iTesselate = (int)tesselateLevel;
		int iTesselate2 = max((int)outerLevel,iTesselate);

		return iTesselate * iTesselate * 3 + (iTesselate2 - iTesselate) * 3;
	}

};


template<class T> void linearDualEdgesTesselator<T>::StripTesselate(const T* vertices, float l, float ol, typename linearDualEdgesTesselator<T>::pushVtxFunctor pushVtx)
{
	float tesselateLevel = Floorf(l);
	float outerLevel = max(Floorf(ol),tesselateLevel);
	
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

	T deltaC = v01;
	deltaC.mul(tesselateLevel);
	deltaC.sub(deltaB);

	pushVtx(prevVtx);
	
	int iTesselate = (int)tesselateLevel;
	int iLevel = iTesselate;

	int iTesselate2 = (int)outerLevel;
	int iLevel2 = iTesselate2;

	T deltaA2;
	T deltaB2;
	
	
	{
		T v01;
		v01 = vertices[1];
		v01.sub(vertices[0]);
		v01.div(outerLevel);

		T v02;
		v02 = vertices[2];
		v02.sub(vertices[0]);
		v02.div(outerLevel);

		T v21;
		v21 = vertices[1];
		v21.sub(vertices[2]);
		v21.div(outerLevel);
		
		deltaA2 = v02;
		deltaB2 = v21;
	}

	vtxDcl currentOuterVtx = currVtx;
	vtxDcl currentInnerVtx = currVtx;

	currentInnerVtx.add(deltaA);

	T innerDelta = deltaA;
	innerDelta.add(deltaB);
	
	T outerDelta = deltaA2;
	outerDelta.add(deltaB2);

	for(int jj=0;jj<iLevel-1;jj++)
	{
		pushVtx(currentOuterVtx);
		currentOuterVtx.add(outerDelta);
		
		pushVtx(currentInnerVtx);
		currentInnerVtx.add(innerDelta);
	}
	iLevel--;
	pushVtx(currentOuterVtx);

	pushVtx(currentInnerVtx);

	for(int i=iLevel;i<iLevel2;i++)
	{
		currentOuterVtx.add(outerDelta);
		pushVtx(currentOuterVtx);
		
		pushVtx(currentInnerVtx);
	}

	deltaA.mul(-1.0f);
	deltaB.mul(-1.0f);
	
	vtxDcl tmp = deltaA;
	deltaA = deltaB;
	deltaB = tmp;

	currVtx = vertices[1];
	currVtx.add(deltaA);
	
	for(int ii=1;ii<iTesselate - 1;ii++)
	{
		pushVtx(currVtx);
		pushVtx(currVtx);
		
		for(int jj=0;jj<iLevel;jj++)
		{
			currVtx.add(deltaA);
			pushVtx(currVtx);

			currVtx.add(deltaB);
			pushVtx(currVtx);
		}
		iLevel--;
		
		deltaA.mul(-1.0f);
		deltaB.mul(-1.0f);
		
		vtxDcl tmp = deltaA;
		deltaA = deltaB;
		deltaB = tmp;
		 
		currVtx.add(deltaA);

	}

	if( iTesselate > 1 )
	{
		pushVtx(currVtx);
		pushVtx(currVtx);

		currVtx.add(deltaA);
		pushVtx(currVtx);

		currVtx.add(deltaB);
		pushVtx(currVtx);
	}
}

template<class T> void linearDualEdgesTesselator<T>::ListTesselate(const T* vertices, float l, float ol, typename linearDualEdgesTesselator<T>::pushTriFunctor pushTri)
{
	float tesselateLevel = Floorf(l);
	float outerLevel = max(Floorf(ol),tesselateLevel);
	
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

	T p1,p2,p3;
	
	int iTesselate = (int)tesselateLevel;
	int iLevel = iTesselate;
	
	int iTesselate2 = (int)outerLevel;
	int iLevel2 = iTesselate2;
	(void)iLevel2;
	
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
	
	T outerDelta = deltaA2;
	outerDelta.add(deltaB2);

	p1 = p2;
	p2 = p3;
	p3 = currentOuterVtx;

	if( iTesselate > 1 )
	{
		p1 = p2;
		p2 = p3;
		p3 = currentInnerVtx;

		currentInnerVtx.add(innerDelta);
		currentOuterVtx.add(outerDelta);

		for(int jj=1;jj<iLevel-1;jj++)
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
		iLevel--;

		p1 = p2;
		p2 = p3;
		p3 = currentOuterVtx;
		pushTri(p1,p2,p3);

		p1 = p2;
		p2 = p3;
		p3 = currentInnerVtx;
		pushTri(p1,p2,p3);
	
		p1 = p2;
		p2 = p3;
		p3 = currentOuterVtx;

		currentOuterVtx.add(outerDelta);
	}
	else
	{
		p1 = vertices[0];
		p2 = vertices[2];

		currentOuterVtx.add(outerDelta);
	}
	
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
	
	if( iTesselate > 1 )
	{
		deltaA.mul(-1.0f);
		deltaB.mul(-1.0f);
		
		vtxDcl tmp = deltaA;
		deltaA = deltaB;
		deltaB = tmp;
		
		currVtx = p2;
		
		for(int ii=1;ii<iTesselate - 1;ii++)
		{
			p1 = currVtx;

			currVtx.add(deltaA);
			p2 = currVtx;

			currVtx.add(deltaB);
			p3 = currVtx;
			
			pushTri(p1,p2,p3);
			
			for(int jj=1;jj<iLevel;jj++)
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
			
			deltaA.mul(-1.0f);
			deltaB.mul(-1.0f);
			
			vtxDcl tmp = deltaA;
			deltaA = deltaB;
			deltaB = tmp;
			 
			currVtx.add(deltaA);
		}

		p1 = currVtx;

		currVtx.add(deltaA);
		p2 = currVtx;

		currVtx.add(deltaB);
		p3 = currVtx;

		pushTri(p1,p2,p3);
	}
	else
	{
		pushTri(p1,p2,p3);
	}
}

#endif // LINEARDUALEDGESTESSELATOR_H