LIBS =  -pthread -lpthread -lrt 
#C_OPTS = -std=gnu99
EXTRA_CFLAGS = $(OPT) $(INCLUDES) -Wno-write-strings
C_FILES := $(wildcard ./*.c)
OBJC_FILES   := $(addprefix obj/,$(notdir $(C_FILES:.c=.o)))

		
TARGET =  readSerialBus rs485Test rs485

all:  $(TARGET)

obj/%.o: ./%.c
	${CC} ${CFLAGS} ${EXTRA_CFLAGS}  ${C_OPTS}  $< -o $@ 

readSerialBus: 
	${CC}   readSerialBus.c  -o  $@ $<

rs485Test:
	${CC}   rs485Test.c  -o  $@ $<

rs485Test:
	${CC}   rs485.c  -o  $@ $<
	
clean:
	rm -f  $(CXX_OBJS) $(TARGET)

obj:
	if [ ! -d  "obj" ]; \
    then \
		mkdir obj;  \
	fi

