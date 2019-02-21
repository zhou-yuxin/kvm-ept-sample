kvm_hybridmem: main.c
	gcc -std=gnu99 main.c -I../include -Ihybridmem/include \
	-Wall -O2 -lm -Lhybridmem/lib -lhybridmem \
	-o kvm_hybridmem

clean:
	rm -f kvm_hybridmem
