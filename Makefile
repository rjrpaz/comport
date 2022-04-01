CC = g++

WINDRES=windres

CFLAGS=-Wall -O3 -mwindows

LIBS = -lodbc32 -lgdi32 -lwsock32 -lmpr

TARGETS = sincro comport patpopup

all: $(TARGETS)

sincro: sincro.c sincro.h
	$(CC) -o sincro $(CFLAGS) sincro.c $(LIBS)

comport: comport.c
	$(CC) -o comport $(CFLAGS) comport.c $(LIBS)

patpopup: patpopup.c patpopup.res
	$(CC) -o patpopup $(CFLAGS) patpopup.c patpopup.res $(LIBS)

patpopup.res: patpopup.rc resource.h
	$(WINDRES) -v $< -O coff -o $@

clean:
	rm -f *.o *.exe *.res

