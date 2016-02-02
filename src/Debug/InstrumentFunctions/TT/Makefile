CFLAGS=-finstrument-functions
all:
    gcc -c -g -finstrument-functions -o  test.o test.c
    gcc -c -g -finstrument-functions  -o  instrument.o instrument.c
    gcc -o test test.o instrument.o
    @echo ok!
clean:
    rm test
