#ifndef VERSION_H
#define VERSION_H

	//Date Version Types
	static const char DATE[] = "02";
	static const char MONTH[] = "11";
	static const char YEAR[] = "2018";

	//Software Status
	static const char STATUS[] = "Beta";
	static const char STATUS_SHORT[] = "b";

	//Standard Version Type
	static const long MAJOR = 0;
	static const long MINOR = 9;
	static const long BUILD = 50;
	static const long REVISION = 1000;

	//Miscellaneous Version Types
	static const long BUILDS_COUNT = 1010;
	#define RC_FILEVERSION 0,9,50,1000
	#define RC_FILEVERSION_STRING "0, 9, 50, 1000\0"
	static const char FULLVERSION_STRING[] = "0.9.50.1000";

#endif //VERSION_h
