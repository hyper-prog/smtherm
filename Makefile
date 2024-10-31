# SMTherm - Smart Manageable Thermostat daemon
# Author: Peter Deak (hyper80@gmail.com)
# License: GPL

CFLAGS= -Wall -g
L_CL_FLAGS= -Wall -g
L_SW_FLAGS= -Wall -g -lm -lwiringPi -lcurl
COMPILER=gcc

all: smtherm

clean:
	rm *.o && rm smtherm

smtherm: smtherm.o dht22.o tools.o therm.o conf.o comm.o history.o drnd.o
	$(COMPILER) $(+) -o $(@) $(L_SW_FLAGS)

smtherm.o: smtherm.c tools.h conf.h therm.h dht22.h drnd.h smtherm.h comm.h history.h
	$(COMPILER) -c $(<) -o $(@) $(CFLAGS)

therm.o: therm.c tools.h conf.h dht22.h smtherm.h comm.h therm.h
	$(COMPILER) -c $(<) -o $(@) $(CFLAGS)

history.o: history.c history.h smtherm.h tools.h
	$(COMPILER) -c $(<) -o $(@) $(CFLAGS)

comm.o: comm.c smtherm.h history.h therm.h tools.h
	$(COMPILER) -c $(<) -o $(@) $(CFLAGS)

tools.o: tools.c tools.h smtherm.h
	$(COMPILER) -c $(<) -o $(@) $(CFLAGS)

conf.o: conf.c smtherm.h therm.h tools.h
	$(COMPILER) -c $(<) -o $(@) $(CFLAGS)

dht22.o: dht22.c dht22.h
	$(COMPILER) -c $(<) -o $(@) $(CFLAGS)

drnd.o: drnd.c drnd.h
	$(COMPILER) -c $(<) -o $(@) $(CFLAGS)

install:
	install -s -m 0755 smtherm /usr/local/bin/smtherm

uninstall:
	rm /usr/local/bin/smtherm
