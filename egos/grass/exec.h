/* Header of an executable.  Followed by a list of segments.
 */
struct exec_header {
	unsigned int eh_base;		// index of first mapped block
	unsigned int eh_offset;		// offset in file (in pages)
	unsigned int eh_size;		// #pages
	unsigned int eh_nsegments;	// #segments
	address_t eh_start;			// initial instruction pointer
};

/* Segments in an executable.
 */
struct exec_segment {
	unsigned int es_first;		// first virtual page
	unsigned int es_npages;		// #pages
	unsigned int es_prot;		// protection bits
};
