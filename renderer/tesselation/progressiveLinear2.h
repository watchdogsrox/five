#ifndef progressiveLinearTesselator2_H
#define progressiveLinearTesselator2_H

template<class T> class progressiveLinearTesselator2
{
public:
	typedef Functor1<const T &> pushVtxFunctor;
	typedef Functor3<const T &,const T &,const T &> pushTriFunctor;

	void StripTesselate(const T* vertices, float level, typename progressiveLinearTesselator2<T>::pushVtxFunctor pushVtx);
	int StripGetVtxCount(float level)
	{
		float tesselateLevel = Floorf(level+1.0f);
		int iTesselate = (int)tesselateLevel;
		
		return iTesselate + (iTesselate) * (iTesselate + 2) - 1 + (( iTesselate > 1 ) ? 0 : 2);
	}
	
	void ListTesselate(const T* vertices, float level, typename progressiveLinearTesselator2<T>::pushTriFunctor pushVtx);
	int ListGetVtxCount	(float level)
	{
		float tesselateLevel = Floorf(level+1.0f);
		int iTesselate = (int)tesselateLevel;

		return iTesselate * iTesselate * 3;
	}

};


template<class T> void progressiveLinearTesselator2<T>::StripTesselate(const T* vertices, float l, typename progressiveLinearTesselator2<T>::pushVtxFunctor pushVtx)
{
	float tesselateLevel = l;
	float tesselateProg = l - Floorf(l);
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
		
	pushVtx(prevVtx);
	
	tesselateLevel = Floorf(l+1.0f);
	int iTesselate = (int)tesselateLevel;
	int iLevel = iTesselate;
	int ii=0;
	vtxDcl tmp;
	
	for(int jj=0;jj<iLevel-1;jj++)
	{
		currVtx.add(deltaA);
		pushVtx(currVtx);

		currVtx.add(deltaB);
		pushVtx(currVtx);
	}
	iLevel--;

	currVtx.add(deltaC);
	pushVtx(currVtx);

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

template<class T> void progressiveLinearTesselator2<T>::ListTesselate(const T* vertices, float l, typename progressiveLinearTesselator2<T>::pushTriFunctor pushTri)
{
	float tesselateLevel = l;
	float tesselateProg = l - Floorf(l);
	zeroDelta = 0.0f == tesselateProg;
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
	p1 = p2 = p3 = prevVtx;
	
	tesselateLevel = Floorf(l+1.0f);
	int iTesselate = (int)tesselateLevel;
	int iLevel = iTesselate;
	int ii=0;
	vtxDcl tmp;
	
	p1 = prevVtx;
	currVtx.add(deltaA);
	p2 = currVtx;

	currVtx.add(deltaB);
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

	currVtx.add(deltaC);
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
	 
	p1 = p3;

	currVtx.add(deltaA);
	p2 = currVtx;
	p3 = currVtx;
	
	for(ii=1;ii<iTesselate-1;ii++)
	{
		bool odd = 1 == (ii & 1);
		
		p1 = p3;

		currVtx.add(deltaA);
		p2 = currVtx;
		
		if( odd )
		{
			currVtx.add(deltaC);
		}
		else
		{
			currVtx.add(deltaB);
		}			
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
	
	if( iTesselate > 1 )
	{
		p1 = p3;
		currVtx.add(progDeltaA);
		p2 = currVtx;
		
		currVtx.add(progDeltaB);
		p3 = currVtx;
		
		pushTri(p1,p2,p3);
	}
}

#endif // progressiveLinearTesselator2_H