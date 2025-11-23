CC= cc
CFLAGS= -Wall
CFLAGS_RELEASE= -Oz
INCS= -I/usr/local/include
LIBS= -L/usr/local/lib -lX11
PREFIX= /usr/local
STRIPPER= llvm-strip
SRC= xsc.c
BIN= xsc

all: release

debug: ${SRC}
	${CC} ${CFLAGS} -o ${BIN} ${INCS} ${LIBS} ${SRC}

release: ${SRC}
	${CC} ${CFLAGS} ${CFLAGS_RELEASE} -o ${BIN} ${INCS} ${LIBS} ${SRC}

strip: ${BIN}
	${STRIPPER} ${BIN}

install: release strip
	mkdir -p ${PREFIX}
	cp -f ${BIN} ${PREFIX}/bin
	chmod 755 ${PREFIX}/bin/${BIN}

clean:
	rm -f ${BIN}
