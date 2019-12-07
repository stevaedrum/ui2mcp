#ifndef VERSION_H
#define VERSION_H

	//Date Version Types
	static const char DATE[] = "07";
	static const char MONTH[] = "12";
	static const char YEAR[] = "2019";
	static const char UBUNTU_VERSION_STYLE[] =  "19.12";
	
	//Software Status
	static const char STATUS[] =  "Release Candidate";
	static const char STATUS_SHORT[] =  "rc";
	
	//Standard Version Type
	static const long MAJOR  = 2;
	static const long MINOR  = 0;
	static const long BUILD  = 1431;
	static const long REVISION  = 7508;
	
	//Miscellaneous Version Types
	static const long BUILDS_COUNT  = 2588;
	#define RC_FILEVERSION 2,0,1431,7508
	#define RC_FILEVERSION_STRING "2, 0, 1431, 7508\0"
	static const char FULLVERSION_STRING [] = "2.0.1431.7508";
	
	//These values are to keep track of your versioning state, don't modify them.
	static const long BUILD_HISTORY  = 51;
	

#endif //VERSION_H
