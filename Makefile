CFLAGS += -Iinclude -std=c11 -Wall -Wextra -pedantic -D_GNU_SOURCE
CFLAGS += -Wmissing-prototypes -Wstrict-prototypes
CFLAGS += -Wold-style-definition -Wpointer-arith -Wcast-qual
CFLAGS += -Wunused -Wno-implicit-fallthrough -fpic
CFLAGS += -Wdouble-promotion -Wfloat-equal
CFLAGS += -Wno-format-nonliteral -Wshadow
CFLAGS += -O2
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

test1: test.c all
	$(CC) test.c -I include -L$(shell pwd) -Wl,-rpath $(shell pwd) -l$(NAME) -o test1 -g
	./test1

test2: test/test.c all
	$(CC) ./test/test.c -I include -L$(shell pwd) -Wl,-rpath $(shell pwd) -l$(NAME) -o ./test/test2 -g
	./test/test.py ./test/test.txt

test3: grep.c all
	$(CC) ./grep.c -O2 -Iinclude/ -L$(shell pwd) -Wl,-rpath	$(shell pwd) -l$(NAME) -o ./grep -g

debug: all
debug: CFLAGS += -g -O0
debug: LDFLAGS += -g -O0

$(TARGET): $(OBJ)
	$(CC) ${LDFLAGS} -o $@ $^

$(SRCS:.c=.d):%.d:%.c
	$(CC) ${LDFLAGS} -o $@ $^

install: all
	cp $(TARGET) /usr/lib/$(TARGET)
	ln -sf /usr/lib/$(TARGET) /usr/lib/lib$(NAME).so
	rsync -av include/* /usr/include/$(NAME)/
	ldconfig
	cp $(NAME) /usr/bin/$(NAME)

clean:
	${RM} ${TARGET} ${OBJ} $(SRC:.c=.d)
	${RM} ./test1
	${RM} ./test/test2

-include $(DEP)
.PHONY: all debug clean
