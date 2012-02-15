#ifndef 	__STRINGBUFFER__
#define 	__STRINGBUFFER__

//__________________________________________________________________________________________

class	StringBuffer {

 char*	sData;
 unsigned long	
        sLength,
		saLength;

 public:
 
	 	StringBuffer 			(void);													
		~StringBuffer			(void);
		
		char*					getString		(void) {return sData;}
		void					appendChar		(const char);
		void					appendBuffer	(const char*);
		void					resetString		(void);
		unsigned long			length			(void) {return sLength;}
		

	static	long				sbDefaultLength,
								sbDefaultBoost;

};

//__________________________________________________________________________________________

class	Vector {

 long*	vData;
 unsigned long
		vLength,
		vaLength;

 public:
 
	 	Vector 			(void);													
		~Vector			(void);
		
		void					appendValue		(const long);
		void					resetVector		(void);
		void					storeValue		(const long, const unsigned long);
		void					storeVector		(const Vector&, const unsigned long);
		void					sort			(void);
		long					value			(const long idx) {return vData[idx];}
		unsigned long			length			(void) {return vLength;}
		

	static	long				vDefaultLength,
								vDefaultBoost;

};


#endif 