#include "tool.h"

int append(std::string & str, const char* lpszFormat, ...){
	char buf[1024 * 64] = {0};
	va_list argList;
	va_start(argList, lpszFormat);
	int nRet = vsnprintf(buf, sizeof(buf), lpszFormat, argList);
    va_end(argList);
	str.append(buf);
	
	return nRet;
}

uint random_32(){
    return (uint)(rand() * 0x1313131);
}

int createUdpSocket(ushort port, int reuse /*= 0*/){
	
	int sock = socket(AF_INET, SOCK_DGRAM, 0);
	if(sock == -1){
		return -1;
	}
	
	if (-1 == setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse))){
		close(sock);
		printf("setsockopt SO_REUSEADDR %d\n", errno);
		return -1;
	}
	
	int i_val = 1024 * 128;
	if (-1 == setsockopt(sock, SOL_SOCKET, SO_SNDBUF, (char*)&i_val, sizeof(i_val))){
		close(sock);
		printf("setsockopt SO_SNDBUF %d\n",sock);
		return -1;
	}	
	
	make_no_block(sock);
	
	sockaddr_in sin ;
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = htonl(INADDR_ANY);
	sin.sin_port = htons(port);

   if (-1 == bind(sock, (struct sockaddr*)&sin, sizeof(sin))) {
	   close(sock);
	   printf("Create bind port %d error\n",port);
	   return -1;
   }
   
   return sock;
}

int64 GetTickCount64U(){
	struct timeval tv = {0};
	gettimeofday(&tv, NULL);
	return ((int64)tv.tv_sec * 1000000) + tv.tv_usec;
}



int globTimerId::GetId(){
	static int idint = ev_sys_user + 11;
	return __sync_fetch_and_add(&idint, 1);
}