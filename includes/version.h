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
	static const long BUILD  = 1206;
	static const long REVISION  = 6297;
	
	//Miscellaneous Version Types
	static const long BUILDS_COUNT  = 2220;
	#define RC_FILEVERSION 1,9,1206,6297
	#define RC_FILEVERSION_STRING "1, 9, 1206, 6297\0"
	static const char FULLVERSION_STRING [] = "1.9.1206.6297";
	
	//These values are to keep track of your versioning state, don't modify them.
	static const long BUILD_HISTORY  = 26;
	

#endif //VERSION_H
