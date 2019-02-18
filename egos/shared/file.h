/* Flags.
 */
#define FILE_ECHO			(1L << 0)

/* permission bits for file access control
 */
#define P_FILE_OTHER_READ			0x1
#define P_FILE_OTHER_WRITE			0x2
#define P_FILE_OWNER_READ			0x4
#define P_FILE_OWNER_WRITE			0x8
#define P_FILE_DEFAULT				(P_FILE_OWNER_READ | P_FILE_OWNER_WRITE | P_FILE_OTHER_READ)
#define P_FILE_ALL					(P_FILE_OWNER_READ | P_FILE_OWNER_WRITE | P_FILE_OTHER_READ | P_FILE_OTHER_WRITE)
// typedef unsigned int mode_t;

struct file_stat {
    bool_t st_alloc;		// set when allocated
    unsigned long st_size;	// size of file in bytes
	unsigned int st_uid;	// owner uid
	mode_t st_mode;			// permission bits
};

struct file_request {
	enum file_op {
		FILE_UNUSED,				// to simplify finding bugs
		FILE_CREATE,
		FILE_CHOWN,
		FILE_CHMOD,
		FILE_READ,
		FILE_WRITE,
		FILE_STAT,					// get status info
		FILE_SETSIZE,				// size is in field offset
		FILE_DELETE,

		/* Special commands for tty server.
		 */
		FILE_SET_FLAGS				// encoded in offset
	} type;							// type of request
	unsigned int ino;				// inode number
	unsigned long offset;			// offset
	unsigned int size;				// amount to read/write
	unsigned int uid;				// uid for chown
	mode_t mode;					// mode for create and chmod
};

struct file_reply {
	enum file_status { FILE_OK, FILE_ERROR } status;
	unsigned int ino;				// for FILE_CREATE only
	enum file_op op;				// operation type in request
	struct file_stat stat;			// information about file
};


bool_t file_exist(gpid_t svr, unsigned int ino);
bool_t file_create(gpid_t svr, mode_t mode, unsigned int *p_ino);
bool_t file_chown(gpid_t svr, unsigned int ino, unsigned int uid);
bool_t file_chmod(gpid_t svr, unsigned int ino, mode_t mode);
bool_t file_read(gpid_t svr, unsigned int ino, unsigned long offset,
										void *addr, unsigned int *psize);
bool_t file_write(gpid_t svr, unsigned int ino, unsigned long offset,
										const void *addr, unsigned int size);
bool_t file_stat(gpid_t svr, unsigned int ino, struct file_stat *pstat);
bool_t file_setsize(gpid_t svr, unsigned int ino, unsigned long size);
bool_t file_delete(gpid_t svr, unsigned int ino);
bool_t file_set_flags(gpid_t svr, unsigned int ino, unsigned long flags);
