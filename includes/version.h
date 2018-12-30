#ifndef VERSION_H
#define VERSION_H

	//Date Version Types
	static const char DATE[] = "30";
	static const char MONTH[] = "12";
	static const char YEAR[] = "2018";
	static const char UBUNTU_VERSION_STYLE[] =  "18.12";
	
	//Software Status
	static const char STATUS[] =  "Beta";
	static const char STATUS_SHORT[] =  "b";
	
	//Standard Version Type
	static const long MAJOR  = 1;
	static const long MINOR  = 3;
	static const long BUILD  = 608;
	static const long REVISION  = 2985;
	
	//Miscellaneous Version Types
	static const long BUILDS_COUNT  = 986;
	#define RC_FILEVERSION 1,3,608,2985
	#define RC_FILEVERSION_STRING "1, 3, 608, 2985\0"
	static const char FULLVERSION_STRING [] = "1.3.608.2985";
	
	//These values are to keep track of your versioning state, don't modify them.
	static const long BUILD_HISTORY  = 28;
	

#endif //VERSION_H
