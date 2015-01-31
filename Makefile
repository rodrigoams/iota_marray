INSTALL_DIR=/usr/local/lib/lua/5.3/iota

.PHONY: clean

iota_marray.so: iota_marray.o
	gcc -shared -o marray.so iota_marray.o -lblas -lm 

iota_marray.o: iota_marray.c
	gcc -Wall -O3 -march=native -std=c99 -fPIC -c -o iota_marray.o iota_marray.c

install: iota_marray.so
	mkdir -p $(INSTALL_DIR)
	cp marray.so $(INSTALL_DIR)

clean:
	rm *.so *.o

