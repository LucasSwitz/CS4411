int _end1;				// this should be loaded last
static int _end2;		// or this

/* Used by malloc() to find the initial "break".
 */
int *end_get(void){
	return &_end1 > &_end2 ? &_end1 : &_end2;
}
