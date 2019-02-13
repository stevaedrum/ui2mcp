#ifndef VERSION_H
#define VERSION_H

	//Date Version Types
	static const char DATE[] = "13";
	static const char MONTH[] = "02";
	static const char YEAR[] = "2019";
	static const char UBUNTU_VERSION_STYLE[] =  "19.02";
	
	//Software Status
	static const char STATUS[] =  "Release Candidate";
	static const char STATUS_SHORT[] =  "rc";
	
	//Standard Version Type
	static const long MAJOR  = 1;
	static const long MINOR  = 9;
	static const long BUILD  = 1225;
	static const long REVISION  = 6411;
	
	//Miscellaneous Version Types
	static const long BUILDS_COUNT  = 2249;
	#define RC_FILEVERSION 1,9,1225,6411
	#define RC_FILEVERSION_STRING "1, 9, 1225, 6411\0"
	static const char FULLVERSION_STRING [] = "1.9.1225.6411";
	
	//These values are to keep track of your versioning state, don't modify them.
	static const long BUILD_HISTORY  = 45;
	

#endif //VERSION_H
