/* Interface to emulated "physical memory".
 */

void *mem_initialize(frame_no nframes, unsigned int prot);
void mem_protect(unsigned int prot);
