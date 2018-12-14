include ../util/config.mk
include config.mk

CFLAGS += -I../util/include
LDFLAGS += ../util/libgwion_ast.a

all: config.mk gwcov gwpp gwtag

config.mk:
	$(info generating config.mk)
	@cp config.mk.orig config.mk

gwcov: gwcov.o
	$(info compiling gwcov)
	@${CC} ${CFLAGS} -lm -o $@ $^

gwpp: gwpp.c
	$(info compiling gwpp)
	@CFLAGS="-DTOOL_MODE -DLINT_MODE" make -C ../util/
	@${CC} ${CFLAGS} -DTOOL_MODE -DLINT_MODE -o $@ $^ ${LDFLAGS}

gwtag: gwtag.c
	$(info compiling gwtag)
	@CFLAGS=-DTOOL_MODE make -C ../util/
	@${CC} ${CFLAGS} -DTOOL_MODE -o $@ $^ ../util/libgwion_ast.a ${LD_FLAGS}

clean:
	@rm gwtag gwpp gwcov *.o
