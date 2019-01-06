#ifndef VERSION_H
#define VERSION_H

	//Date Version Types
	static const char DATE[] = "06";
	static const char MONTH[] = "01";
	static const char YEAR[] = "2019";
	static const char UBUNTU_VERSION_STYLE[] =  "19.01";
	
	//Software Status
	static const char STATUS[] =  "Beta";
	static const char STATUS_SHORT[] =  "b";
	
	//Standard Version Type
	static const long MAJOR  = 1;
	static const long MINOR  = 6;
	static const long BUILD  = 881;
	static const long REVISION  = 4460;
	
	//Miscellaneous Version Types
	static const long BUILDS_COUNT  = 1528;
	#define RC_FILEVERSION 1,6,881,4460
	#define RC_FILEVERSION_STRING "1, 6, 881, 4460\0"
	static const char FULLVERSION_STRING [] = "1.6.881.4460";
	
	//These values are to keep track of your versioning state, don't modify them.
	static const long BUILD_HISTORY  = 1;
	

#endif //VERSION_H
