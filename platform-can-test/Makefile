SKIN=alchemy
MAIN_SRC=platform-can-test
TARGET=platform-can-test

LM=-lm
GSL_CFLAGS=-I/usr/include/gsl
GSL_LDFLAGS=-L/usr/lib -lgsl -lgslcblas -lm

CFLAGS := $(shell xeno-config --alchemy --rtdm --cflags)
LDFLAGS := $(LM) $(shell xeno-config --alchemy --rtdm --ldflags)
CC := $(shell xeno-config --cc)

$(TARGET): $(MAIN_SRC).c
	$(CC) -o $@ $< $(CFLAGS) $(GSL_CFLAGS) $(LDFLAGS) $(GSL_LDFLAGS)
