#以下是指定编译器路径
CC      := g++

AR := ar rc

# args

#以下是指定需要的库文件 -L
LIBS    := -lrt

#以下是指定编译需要的头文件
INCLUDE := -I./

#以下是lib路径
LIBPATH := 


#以下是源文件
SRCS    := *.cpp

#以下是指定目标文件 所有当.cpp文件变成.o文件
OBJS    := $(SRCS:.cpp=.o)


#以下是编译选项
CFLAGS  := -g -Wall -c $(INCLUDE) $(LIBPATH) 
CFLAGS  += -DLINUX 

TARGETLIB := libserver.a

all:
	$(CC) $(CFLAGS) $(SRCS) $(LIBS)
	$(AR) $(TARGETLIB) $(OBJS) 
#make clean 删除所有的.o文件
clean:
	rm -f ./*.o
