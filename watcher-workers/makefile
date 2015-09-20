#以下是指定编译器路径
CC      := g++

#以下是指定编译需要的头文件
INCLUDE := -I./ -I../common/

#以下是指定需要的库文件 -L
LIBS    := -lrt

#以下是指定需要的库文件路径
LIBPATH := 


#以下是源文件
#wildcard把 指定目录 ./ 和 ../common/ 下的所有后缀是cpp的文件全部展开。
SRCS    := $(wildcard *.cpp ../common/*.cpp)

#notdir把展开的文件去除掉路径信息
DIR = $(notdir $(SRCS))

#以下是指定目标文件 所有当.cpp文件变成.o文件
OBJS    := $(DIR:.cpp=.o)


#以下是编译选项
CFLAGS  := -g -Wall $(INCLUDE) $(LIBPATH) 

#以下是生成可执行文件
TARGET  := reactor

#1编译器 2编译选项 3输出 4生成的可执行文件 5需要的源文件 6需要当库文件
all:
	$(CC) $(CFLAGS) -o $(TARGET) $(SRCS) $(LIBS)

#make clean 删除所有的.o文件
clean:
	rm -f ./*.o
