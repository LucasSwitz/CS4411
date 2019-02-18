#include "egos.h"
#include "string.h"
#include "file.h"
#include "dir.h"

FILE *stdin, *stdout, *stderr;

/* Allocate a new FILE for the given fid.
 */
FILE *falloc(fid_t fid, unsigned int flags){
	FILE *fp = (FILE *) calloc(1, sizeof(FILE));

	fp->fid = fid;
	fp->flags = flags;
	return fp;
}

/* Lookup the path name and return its directory and file identifier.
 * Returns False if unsuccessful.  However, *p_dir may still be set
 * to the parent directory if only the last component of the file
 * name could not be mapped.
 */
bool_t flookup(const char *restrict path, bool_t is_dir,
									fid_t *p_dir, fid_t *p_fid){
	/* See if this is an absolute or relative path.
	 */
	fid_t dir;
	if (*path == '/') {
		dir = GRASS_ENV->root;
		while (*path == '/') {
			path++;
		}
	}
	else {
		dir = GRASS_ENV->cwd;
	}

	/* Now walk the path until the final component.
	 */
	bool_t success = True;
	fid_t fid = dir;
	while (*path != 0) {
		/* See if there's more.
		 */
		char *e = strchr(path, '/'), *next;
		if (e == 0) {
			if (is_dir) {
				unsigned int n = strlen(path);
				next = malloc(n + 5);
				memcpy(next, path, n);
				strcpy(&next[n], ".dir");
			}
			else {
				next = (char *) path;
			}
		}
		else {
			next = malloc((e - path) + 5);
			memcpy(next, path, e - path);
			strcpy(&next[e - path], ".dir");
		}

		/* Take the next step.
		 */
		success = dir_lookup(GRASS_ENV->servers[GPID_DIR], dir, next, &fid);
		if (e != 0 || is_dir) {
			free(next);
		}
		if (e == 0) {
			if (p_dir != 0) {
				*p_dir = dir;
			}
			if (!success) {
				return False;
			}
			break;
		}

		/* Go on to the remainder of the path.
		 */
		if (!success) {
			return False;
		}
		path = e + 1;
		dir = fid;
	}
	if (p_dir != 0) {
		*p_dir = dir;
	}
	if (p_fid != 0) {
		*p_fid = fid;
	}
	return True;
}

/* TODO.  Interpret mode.  Think about create, truncate, and append, and
 *		  set the position correctly in the FILE structure.
 */
FILE *fopen(const char *restrict path, const char *restrict mode){
	unsigned int flags = 0;
	bool_t create = False, truncate = False;

	switch (mode[0]) {
	case 'r':
		flags |= _FILE_READ;
		if (mode[1] == '+') {
			flags |= _FILE_WRITE;
		}
		break;
	case 'w':
		flags |= _FILE_WRITE;
		create = truncate = True;
		if (mode[1] == '+') {
			flags |= _FILE_READ;
		}
		break;
	case 'a':
		flags |= _FILE_WRITE | _FILE_APPEND;
		create = True;
		if (mode[1] == '+') {
			flags |= _FILE_READ;
		}
	default:
		fprintf(stderr, "fopen: don't understand mode\n");
		return 0;
	}

	fid_t dir, fid;
	dir = fid_val(0, 0);
	bool_t success = flookup(path, False, &dir, &fid);

	/* See if the file exists.  If not and the file isn't opened just for
	 * reading, try to create it.
	 */
	if (!success) {
		if (!create || dir.server == 0) {
			return 0;
		}

		/* Find the last component in path.
		 */
		const char *last = rindex(path, '/');
		if (last == 0) {
			last = path;
		}
		else if (*++last == 0) {
			return 0;
		}

		/* Use the parent directory's file server as the file server.
		 */
		fid.server = dir.server;
		if (!file_create(dir.server, P_FILE_DEFAULT, &fid.ino)) {
			return 0;
		}

		/* Insert into the directory.
		 */
		if (!dir_insert(GRASS_ENV->servers[GPID_DIR], dir, last, fid)) {
			return 0;
		}
	}
	if (truncate) {
		file_setsize(fid.server, fid.ino, 0);
	}
	return falloc(fid, flags);
}

FILE *freopen(const char *restrict filename, const char *restrict mode, FILE *restrict stream){
	fprintf(stderr, "freopen: not yet implemented\n");
	return 0;
}

size_t fread(void *restrict ptr, size_t size, size_t nitems, FILE *restrict stream){
	if (stream->flags & (_FILE_EOF | _FILE_ERROR)) {
		return 0;
	}

	unsigned int n = size * nitems;
	bool_t r = file_read(stream->fid.server, stream->fid.ino, stream->pos,
										ptr, &n);
	if (!r) {
		printf("fread: file_read returned an error\n");
		stream->flags |= _FILE_ERROR;
		return 0;
	}
	if (n == 0) {
		stream->flags |= _FILE_EOF;
		return 0;
	}
	if (n % size != 0) {
		printf("fread: returned %d instead of %d * %d\n", n, (int) size, (int) nitems);
		stream->flags |= _FILE_ERROR;
		return 0;
	}
	stream->pos += n;
	return n / size;
}

size_t fwrite(const void *restrict ptr, size_t size, size_t nitems, FILE *restrict stream){
	if (stream->flags & _FILE_ERROR) {
		return 0;
	}

	unsigned int n = size * nitems;
	bool_t r = file_write(stream->fid.server, stream->fid.ino,
// TODO			(stream->flags & _FILE_APPEND) ? _FILE_APPEND_POS : stream->pos,
			stream->pos, ptr, n);
	if (!r) {
		printf("fwrite: file_write returned an error\n");
		stream->flags |= _FILE_ERROR;
		return 0;
	}
	stream->pos += n;
	return n;
}

int fclose(FILE *stream){
	free(stream);
	return 0;
}

int fgetc(FILE *stream){
	char c;
	int n = fread(&c, 1, 1, stream);

	if (n < 0) {
		fprintf(stderr, "fgetc: error on input\n");
		return EOF;
	}
	if (n == 0) {
		return EOF;
	}
	return c & 0xFF;
}

char *fgets(char *restrict str, int size, FILE *restrict stream){
	int c, i;

	for (i = 0; i < size - 1;) {
		if ((c = fgetc(stream)) == EOF) {
			if (i == 0) {
				return 0;
			}
			break;
		}
		str[i++] = c;
		if (c == '\n') {
			break;
		}
	}
	str[i] = 0;
	return str;
}

int fputs(const char *restrict s, FILE *restrict stream){
	int n = strlen(s);

	return fwrite(s, 1, n, stream);
}

int fputc(int c, FILE *stream){
	return fwrite(&c, 1, 1, stream) == 1 ? c : EOF;
}

int fprintf(FILE *restrict stream, const char *fmt, ...){
	va_list ap;
	struct mem_chan *mc = mc_alloc();

	va_start(ap, fmt);
	mc_vprintf(mc, fmt, ap);
	va_end(ap);

	// int size = fwrite(mc->buf, 1, mc->offset, stream);
	int size = file_write(GRASS_ENV->stderr.server, GRASS_ENV->stderr.ino, 0, mc->buf, mc->offset);

	mc_free(mc);
	return size;
}

int fgetpos(FILE *restrict stream, fpos_t *restrict pos){
	pos->offset = stream->pos;
	return 0;
}

int fsetpos(FILE *stream, const fpos_t *pos){
	stream->pos = pos->offset;
	return 0;
}

int fseek(FILE *stream, long offset, int whence){
	switch (whence) {
	case SEEK_SET:
		stream->pos = offset;
		stream->flags &= ~_FILE_EOF;
		return 0;
	case SEEK_CUR:
		stream->pos += offset;
		stream->flags &= ~_FILE_EOF;
		return 0;
	case SEEK_END:
		{
			struct file_stat stat;

			if (!file_stat(stream->fid.server, stream->fid.ino, &stat)) {
				fprintf(stderr, "fseek: can't stat\n");
				return -1;
			}
			stream->pos = stat.st_size + offset;
			stream->flags &= ~_FILE_EOF;
		}
		return 0;
	default:
		fprintf(stderr, "fseek: bad offset\n");
		stream->pos |= _FILE_ERROR;
		return -1;
	}
}

long ftell(FILE *stream){
	return (long) stream->pos;
}

/* Everything is always flushed currently.
 */
int fflush(FILE *stream){
	return 0;
}

int feof(FILE *stream){
	return stream->flags & _FILE_EOF;
}

int ferror(FILE *stream){
	return stream->flags & _FILE_ERROR;
}

int getchar(void){
	return fgetc(stdin);
}

int putc(int c, FILE *stream){
	return fputc(c, stream);
}

int puts(const char *s){
	if (fputs(s, stdout) >= 0) {
		return putchar('\n');
	}
	return EOF;
}

int putchar(int c){
	return fputc(c, stdout);
}

int ungetc(int c, FILE *stream){
	fprintf(stderr, "ungetc: not yet implemented\n");
	return EOF;
}

int fileno(FILE *stream){
	fprintf(stderr, "fileno: not implemented\n");
	return -1;
}

void clearerr(FILE *stream){
	stream->flags &= ~(_FILE_EOF | _FILE_ERROR);
}

void setbuf(FILE *restrict stream, char *restrict buf){
	/* ignore */
}

int setvbuf(FILE *restrict stream, char *restrict buf, int type, size_t size){
	return 0;
}

void perror(const char *s){
	fprintf(stderr, "perror: %s\n", s);
}

void rewind(FILE *stream){
	fseek(stream, 0, SEEK_SET);
}

int remove(const char *path){
	fprintf(stderr, "remove: not yet implemented\n");
	return -1;
}

int rename(const char *old, const char *new){
	fprintf(stderr, "rename: not yet implemented\n");
	return -1;
}
 
FILE *tmpfile(void){
	fprintf(stderr, "tmpfile: not yet implemented\n");
	return 0;
}

void exit(int status){
	sys_exit(status);
	fprintf(stderr, "exit: returned???\n");
	for (;;);
}

void stdio_finit(void){
	stdin = falloc(GRASS_ENV->stdin, _FILE_READ);
	stdout = falloc(GRASS_ENV->stdout, _FILE_WRITE | _FILE_APPEND);
	stderr = falloc(GRASS_ENV->stderr, _FILE_WRITE | _FILE_APPEND);
}

#ifdef notdef

int stat(const char *restrict path, struct stat *restrict buf){
	fid_t fid;
	uint64_t size;

	if (grass_map(path, &fid) < 0) {
		fprintf(stderr, "stat: unknown file '%s'\n", path);
		return -1;
	}
	if (grass_getsize(fid, &size) < 0) {
		fprintf(stderr, "stat: can't get size of '%s'\n", path);
		return -1;
	}
	buf->st_dev = fid.fh_pid;
	buf->st_ino = fid.fh_ino;
	buf->st_size = size;
	return 0;
}

#endif
