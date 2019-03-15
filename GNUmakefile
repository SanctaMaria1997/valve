all: valve libvalve.so example manpage depend

valve: valve.o valve_util.o elf_util.o
	cc valve.o valve_util.o elf_util.o -o valve
valve.o: valve.c
	cc -c -DLINUX valve.c -o valve.o 
libvalve.so: libvalve.o dwarfy.o valve_util.o elf_util.o
	cc -shared -fPIC libvalve.o dwarfy.o valve_util.o elf_util.o -o libvalve.so -ldl
libvalve.o: libvalve.c
	cc -c -fPIC -DLINUX libvalve.c -o libvalve.o
dwarfy.o: dwarfy.c
	cc -c -fPIC -DLINUX dwarfy.c -o dwarfy.o
valve_util.o: valve_util.c
	cc -c -fPIC valve_util.c -o valve_util.o
elf_util.o: elf_util.c
	cc -c -fPIC -DLINUX elf_util.c -o elf_util.o
dwarfy_test: dwarfy_test.c
	cc -g dwarfy_test.c dwarfy.o -o dwarfy_test
example: fish.o hamster.o libdugong.so
	cc -g fish.o hamster.o -o example -ldugong
libdugong.so: dugong.o
	cc -g -shared -fPIC dugong.o -o libdugong.so
fish.o: fish.c
	cc -c -g fish.c -o fish.o
hamster.o: hamster.c
	cc -c -g hamster.c -o hamster.o
dugong.o: dugong.c
	cc -c -g -fPIC dugong.c -o dugong.o

manpage: valve.1
	gzip -f -k valve.1
install:
	cp valve /usr/local/bin
	cp libvalve.so /usr/local/lib
	cp libdugong.so /usr/local/lib
	cp valve.1.gz /usr/share/man/man1/
depend:
	cc -E -MM *.c > .depend
clean:
	rm *.o *.so valve example valve.1.gz
