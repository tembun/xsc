CC= cc
CFLAGS= -Wall
CFLAGS_RELEASE= -Oz
INCS= -I/usr/local/include
LIBS= -L/usr/local/lib -lX11
STRIPPER= llvm-strip
SRC= xsc.c
BIN= xsc

all: release

debug: ${SRC}
	${CC} ${CFLAGS} -o ${BIN} ${INCS} ${LIBS} ${SRC}

release: xsc.c
	${CC} ${CFLAGS} ${CFLAGS_RELEASE} -o ${BIN} ${INCS} ${LIBS} ${SRC} && \
	    ${STRIPPER} ${BIN}
