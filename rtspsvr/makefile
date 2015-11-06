#������ָ��������·��
CC      := g++

#������ָ����Ҫ�Ŀ��ļ� -L
LIBS    := -lrt -ldl -lpthread -lserver2

#������ָ��������Ҫ��ͷ�ļ�
INCLUDE := -I./ -I../include  -I./h264 
#������lib·��
LIBPATH := -L../lib


#������Դ�ļ�
SRCS    := *.cpp

#������ָ��Ŀ���ļ� ���е�.cpp�ļ�����.o�ļ�
OBJS    := $(SRCS:.cpp=.o)


#�����Ǳ���ѡ��
CFLAGS  := -g -Wall -O $(INCLUDE) $(LIBPATH) 
CFLAGS  += -DLINUX  -Wno-unused-function

#���������ɿ�ִ���ļ�
TARGET  := rtspsvr


#1������ 2����ѡ�� 3���� 4���ɵĿ�ִ���ļ� 5��Ҫ��Դ�ļ� 6��Ҫ�����ļ�
all:
	$(CC) $(CFLAGS) -o $(TARGET) $(SRCS) $(LIBS)

#make clean ɾ�����е�.o�ļ�
clean:
	rm -f ./*.o
