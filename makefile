#以下是指定编译器路径
CC      := g++

AR := ar rc

# args
BITS := 32
DEBUG := 1
T := .a

#以下是指定需要的库文件 -L
LIBS    := -lrt -lpthread

#以下是指定编译需要的头文件
INCLUDE := -I./

#以下是lib路径
LDFLAGS :=

#以下是源文件
SRCS    := *.cpp

#以下是指定目标文件 所有当.cpp文件变成.o文件
OBJS    := $(SRCS:.cpp=.o)

#以下是编译选项
CFLAGS  := -Wall $(INCLUDE) 

#[args]  make DEBUG=1
ifeq ($(DEBUG),1)
	CFLAGS += -g
else
	CFLAGS += -DNDEBUG
endif

#[args]  make BITS=32
ifeq ($(BITS),64)
	CFLAGS += -m64
else
	CFLAGS += -m32
endif


#.a
ifeq ("$(T)", ".a")
  CFLAGS += -c
  TARGET = libserver.a
  target_lib:
	$(CC) $(CFLAGS) $(SRCS) $(LDFLAGS) $(LIBS)
	$(AR) $(TARGET) $(OBJS) 
endif	

#so
ifeq ("$(T)", "so")
  TARGET := libserver.so
  CFLAGS += -o
  LDFLAGS += -fPIC -shared
  target_so:
	$(CC) $(CFLAGS) $(TARGET) $(SRCS) $(LDFLAGS) $(LIBS) 
endif

#make clean 删除所有的.o文件
clean:
	rm -f ./*.o
