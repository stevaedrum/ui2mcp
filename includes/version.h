#ifndef VERSION_H
#define VERSION_H

	//Date Version Types
	static const char DATE[] = "16";
	static const char MONTH[] = "12";
	static const char YEAR[] = "2018";
	static const char UBUNTU_VERSION_STYLE[] =  "18.12";
	
	//Software Status
	static const char STATUS[] =  "Beta";
	static const char STATUS_SHORT[] =  "b";
	
	//Standard Version Type
	static const long MAJOR  = 0;
	static const long MINOR  = 9;
	static const long BUILD  = 175;
	static const long REVISION  = 533;
	
	//Miscellaneous Version Types
	static const long BUILDS_COUNT  = 176;
	#define RC_FILEVERSION 0,9,175,533
	#define RC_FILEVERSION_STRING "0, 9, 175, 533\0"
	static const char FULLVERSION_STRING [] = "0.9.175.533";
	
	//These values are to keep track of your versioning state, don't modify them.
	static const long BUILD_HISTORY  = 95;
	

#endif //VERSION_H
