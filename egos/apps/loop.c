#include "egos.h"
#include "string.h"

int main(int argc, char **argv){
	volatile unsigned long x;

	printf("%u: start looping\n", sys_getpid());
	if (argc > 1) {
		x = atol(argv[1]);
		if (x == 0) {
			for (;;) ;
		}
	}
	else {
		x = 5000000000;
	}
	while (--x != 0) ;
	return 0;
}
