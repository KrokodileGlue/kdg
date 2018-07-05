CFLAGS += -O2 -Iinclude -std=c11 -Wall -Wextra -pedantic
CFLAGS += -Wunused -Wno-implicit-fallthrough -fpic
LDFLAGS += -Wl,--as-needed,-O2,-z,relro,-z,now -shared

MAJOR := 0
MINOR := 1
NAME := kdg
VERSION := $(MAJOR).$(MINOR)
TARGET := lib$(NAME).so.$(VERSION)

SRC := $(wildcard src/*.c)
OBJ := $(SRC:.c=.o)
DEP := $(OBJ:.o=.d)

all: $(TARGET)
	cp $(TARGET) lib$(NAME).so
	$(CC) main.c -I include -L$(shell pwd) -Wl,-rpath $(shell pwd) -l$(NAME) -o $(NAME) -g

test: test.c all
	$(CC) test.c -I include -L$(shell pwd) -Wl,-rpath $(shell pwd) -l$(NAME) -o test -g
	./test

debug: all
debug: CFLAGS += -g -O0
debug: LDFLAGS += -g -O0

$(TARGET): $(OBJ)
	$(CC) ${LDFLAGS} -o $@ $^

$(SRCS:.c=.d):%.d:%.c
	$(CC) ${LDFLAGS} -o $@ $^

install:
	cp $(TARGET) /usr/local/lib/$(TARGET)
	ln -sf /usr/local/lib/$(TARGET) /usr/local/lib/lib$(NAME).so
	rsync -av include/* /usr/local/include/$(NAME)/
	ldconfig
	cp $(NAME) /usr/local/bin/$(NAME)

clean:
	${RM} ${TARGET} ${OBJ} $(SRC:.c=.d)
	${RM} test

-include $(DEP)
.PHONY: all debug clean
