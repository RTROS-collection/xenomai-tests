SKIN=alchemy
MAIN_SRC=gsl_lorenz_udp
TARGET=gsl_lorenz_udp

LM=-lm
GSL_CFLAGS=-I/usr/include/gsl
GSL_LDFLAGS=-L/usr/lib -lgsl -lgslcblas -lm

CFLAGS := $(shell xeno-config --skin=alchemy --cflags)
LDFLAGS := $(LM) $(shell xeno-config --skin=alchemy --ldflags)
CC := $(shell xeno-config --cc)

$(TARGET): $(MAIN_SRC).c
	$(CC) -o $@ $< $(CFLAGS) $(GSL_CFLAGS) $(LDFLAGS) $(GSL_LDFLAGS)
