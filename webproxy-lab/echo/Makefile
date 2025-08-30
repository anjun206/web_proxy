# echo/Makefile  — echoserver & echoclient 빌드
CC       := gcc
CFLAGS   := -Wall -Wextra -g -O0 -I.. -MMD -MP
LDFLAGS  :=
RM       := rm -f

# 객체 파일 목록
SRV_OBJS := echo_srv.o echo.o ../csapp.o
CLI_OBJS := echo_cli.o        ../csapp.o

# .d 의존성 파일들(현재 폴더 것만 포함)
DEPS := $(SRV_OBJS:.o=.d) $(CLI_OBJS:.o=.d)
DEPS := $(filter %.d,$(DEPS))

.PHONY: all clean
all: echoserver echoclient

# 링크 단계
echoserver: $(SRV_OBJS)
	$(CC) $(SRV_OBJS) -o $@ $(LDFLAGS)

echoclient: $(CLI_OBJS)
	$(CC) $(CLI_OBJS) -o $@ $(LDFLAGS)

# 공통 컴파일 규칙 (.c -> .o)
# -MMD -MP 덕분에 각 .o 옆에 .d(의존성) 자동 생성
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# 상위 폴더의 csapp.o 규칙
../csapp.o: ../csapp.c ../csapp.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	$(RM) *.o *.d echoserver echoclient

# 자동 의존성 포함 (없어도 에러 안 나게 -)
-include $(DEPS)
