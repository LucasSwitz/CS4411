struct spawn_request {
	enum {
		SPAWN_UNUSED,				// simplifies finding bugs
		SPAWN_EXEC,
		SPAWN_KILL,
		SPAWN_GETUID
	} type;							// type of request

	union {
		struct {
			fid_t executable;		// identifies executable
			unsigned int size;		// size of args/env that follows request
			bool_t interruptable;	// foreground process
			unsigned int uid;		// requested uid
		} exec;
		struct {
			gpid_t pid;
			int status;
		} kill;
		struct {
			gpid_t pid;
		} getuid;
	} u;
};

struct spawn_reply {
	enum spawn_status { SPAWN_OK, SPAWN_ERROR } status;
	union {
		gpid_t pid;					// process id of process
		unsigned int uid;			// user id of process
	} u;
};

bool_t spawn_exec(gpid_t svr, fid_t executable, const char *args, unsigned int size,
						bool_t interruptable, unsigned int uid, gpid_t *ppid);
bool_t spawn_kill(gpid_t svr, gpid_t pid, int status);
bool_t spawn_load_args(const struct grass_env *ge_init,
							int argc, char *const *argv,
							char **p_argb, unsigned int *p_size);

/* TODO.  This doesn't really belong here since it is user-space only.
 * Maybe lib/spawn.h
 */
bool_t spawn_fexec(const char *exec, const char *argb, int total, bool_t interruptable,
										unsigned int uid, gpid_t *ppid);
bool_t spawn_vexec(const char *exec, int argc, char *const *argv, bool_t interruptable,
										unsigned int uid, gpid_t *ppid);
