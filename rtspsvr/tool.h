#pragma once

#include <string>
#include <stdarg.h> 
#include <stdio.h>
#include <sys/stat.h>


#include "utility.h"
typedef unsigned char  uchar;
typedef unsigned char  uint8;
typedef unsigned short uint16;

typedef std::string::size_type size_type;

#define safe_del(x) if(x){delete x; x= NULL;}

int append(std::string & str, const char* lpszFormat, ...);

uint random_32();

int createUdpSocket(ushort port, int reuse = 0);

int64 GetTickCount64U();

class IStreamCtrl{
public:
	virtual int TransferStream(uchar * data, int len, bool end_of_frame) = 0;;
};

class ITimerTask{
public:
	virtual int add_delay_task(int id, int delay, void * context = NULL) = 0;
};

class globTimerId{
public:
	static int GetId();
};