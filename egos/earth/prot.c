#include <stdint.h>
#include <sys/mman.h>
#include "earth.h"
#include "myalloc.h"
#include "prot.h"

/* Convert P_READ etc. to PROT_READ etc.
 */
unsigned int prot_cvt(unsigned int prot){
	unsigned int result = 0;

	if (prot & P_READ) {
		result |= PROT_READ;
	}
	if (prot & P_WRITE) {
		result |= PROT_WRITE;
	}
	if (prot & P_EXEC) {
		result |= PROT_EXEC;
	}
	return result;
}
