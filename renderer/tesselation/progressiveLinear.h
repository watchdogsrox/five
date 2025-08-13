#ifndef PROGRESSIVELINEARTESSELATOR_H
#define PROGRESSIVELINEARTESSELATOR_H

template<class T> class progressiveLinearTesselator
{
public:
	typedef Functor1<const T &> pushVtxFunctor;
	typedef Functor3<const T &,const T &,const T &> pushTriFunctor;

	void StripTesselate(const T* vertices, float level, pushVtxFunctor pushVtx);
	int StripGetVtxCount(float level)
	{
		float tesselateLevel = Floorf(level+1.0f);
		int iTesselate = (int)tesselateLevel;
		
		return iTesselate + (iTesselate) * (iTesselate + 2) - 1 + (( iTesselate > 1 ) ? 0 : 2);
	}
	
	void ListTesselate(const T* vertices, float level, pushTriFunctor pushVtx);
	int ListGetVtxCount	(float level)
	{
		float tesselateLevel = Floorf(level+1.0f);
		int iTesselate = (int)tesselateLevel;

		return iTesselate * iTesselate * 3;
	}

};


template<class T> void progressiveLinearTesselator<T>::StripTesselate(const T* vertices, float l, typename progressiveLinearTesselator<T>::pushVtxFunctor pushVtx)
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

	currVtx.add(progDeltaA);
	pushVtx(currVtx);

	currVtx.add(progDeltaB);
	pushVtx(currVtx);
	
	deltaA.mul(-1.0f);
	deltaB.mul(-1.0f);
	
	progDeltaA.mul(-1.0f);
	progDeltaB.mul(-1.0f);
	
	tmp = deltaA;
	deltaA = deltaB;
	deltaB = tmp;

	tmp = progDeltaA;
	progDeltaA = progDeltaB;
	progDeltaB = tmp;
	 
	currVtx.add(progDeltaA);
	pushVtx(currVtx);
	pushVtx(currVtx);

	for(ii=1;ii<iTesselate-1;ii++)
	{
		bool odd = 1 == (ii & 1);
		
		currVtx.add(deltaA);
		pushVtx(currVtx);

		if( odd )
		{
			currVtx.add(progDeltaB);
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
			currVtx.add(progDeltaA);
			pushVtx(currVtx);
		}

		currVtx.add(deltaB);
		pushVtx(currVtx);			
					
		deltaA.mul(-1.0f);
		deltaB.mul(-1.0f);
	
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
		if( iTesselate & 1 )
		{
			currVtx.add(progDeltaA);
			pushVtx(currVtx);

			currVtx.add(deltaB);
			pushVtx(currVtx);			
		}
		else
		{
			currVtx.add(deltaA);
			pushVtx(currVtx);

			currVtx.add(progDeltaB);
			pushVtx(currVtx);			
		}
	}
}

template<class T> void progressiveLinearTesselator<T>::ListTesselate(const T* vertices, float l, typename progressiveLinearTesselator<T>::pushTriFunctor pushTri)
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
	
	T p1,p2,p3;
	
	tesselateLevel = Floorf(l+1.0f);
	int iTesselate = (int)tesselateLevel;
	int iLevel = iTesselate;
	int ii=0;
	T tmp;
	

	p1 = currVtx;
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

	currVtx.add(progDeltaA);
	p1 = p2;
	p2 = p3;
	p3 = currVtx;
	pushTri(p1,p2,p3);

	currVtx.add(progDeltaB);
	p1 = p2;
	p2 = p3;
	p3 = currVtx;
	pushTri(p1,p2,p3);

	deltaA.mul(-1.0f);
	deltaB.mul(-1.0f);
	
	progDeltaA.mul(-1.0f);
	progDeltaB.mul(-1.0f);
	
	tmp = deltaA;
	deltaA = deltaB;
	deltaB = tmp;

	tmp = progDeltaA;
	progDeltaA = progDeltaB;
	progDeltaB = tmp;
	 
	p1 = p3;

	currVtx.add(progDeltaA);
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
			currVtx.add(progDeltaB);
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
		else
		{
			currVtx.add(progDeltaA);
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
					
		deltaA.mul(-1.0f);
		deltaB.mul(-1.0f);
	
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
	}
	
	if( iTesselate > 1 )
	{
		p1 = p3;

		if( iTesselate & 1 )
		{
			currVtx.add(progDeltaA);
			p2 = currVtx;

			currVtx.add(deltaB);
			p3 = currVtx;
		}
		else
		{
			currVtx.add(deltaA);
			p2 = currVtx;

			currVtx.add(progDeltaB);
			p3 = currVtx;
		}

		pushTri(p1,p2,p3);
	}
}

#endif // PROGRESSIVELINEARTESSELATOR_H