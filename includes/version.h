#ifndef VERSION_H
#define VERSION_H

	//Date Version Types
	static const char DATE[] = "05";
	static const char MONTH[] = "03";
	static const char YEAR[] = "2019";
	static const char UBUNTU_VERSION_STYLE[] =  "19.03";
	
	//Software Status
	static const char STATUS[] =  "Release Candidate";
	static const char STATUS_SHORT[] =  "rc";
	
	//Standard Version Type
	static const long MAJOR  = 2;
	static const long MINOR  = 0;
	static const long BUILD  = 1389;
	static const long REVISION  = 7270;
	
	//Miscellaneous Version Types
	static const long BUILDS_COUNT  = 2522;
	#define RC_FILEVERSION 2,0,1389,7270
	#define RC_FILEVERSION_STRING "2, 0, 1389, 7270\0"
	static const char FULLVERSION_STRING [] = "2.0.1389.7270";
	
	//These values are to keep track of your versioning state, don't modify them.
	static const long BUILD_HISTORY  = 9;
	

#endif //VERSION_H
