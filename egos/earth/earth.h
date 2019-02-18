/* Interface to underlying virtualize hardware.
 */

#define VIRT_PAGES	((address_t) 0x100)				// size of virtual address space

#ifdef MACOSX
#define VIRT_BASE	((address_t) 0x1000000000)		// base of this space
#endif
#ifdef LINUX
#define VIRT_BASE	((address_t) 0x10000000)		// base of this space
#endif

#define PAGESIZE	4096

/* Handy.
 */
#define VIRT_TOP		(VIRT_BASE + VIRT_PAGES * PAGESIZE)

#define BLOCK_SIZE      1024         // # bytes in a disk block

/* An "address_t" is an unsigned integer that is the size of a pointer.
 */
#ifdef x86_32
typedef uint32_t address_t;
#define PRIaddr PRIx32
#endif
#ifdef x86_64
typedef uint64_t address_t;
#define PRIaddr PRIx64
#endif

typedef uint32_t page_no;		// virtual page number
typedef uint32_t frame_no;		// physical frame number

/* Protection bits.  Do not necessarily correspond to PROT_READ etc.
 */
#define P_READ			0x1
#define P_WRITE			0x2
#define P_EXEC			0x4

typedef enum { False, True } bool_t;
