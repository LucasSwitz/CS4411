/* Interface to emulated TLB device.
 */

void tlb_initialize(unsigned int nentries);
void tlb_flush(void);
void tlb_sync(void);
int tlb_map(unsigned int tlb_index, page_no virt, void *phys,
								unsigned int prot);
int tlb_get(unsigned int tlb_index, page_no *virt,
								void **phys, unsigned int *prot);
void tlb_unmap(page_no virt);
int tlb_get_entry(page_no virt);
