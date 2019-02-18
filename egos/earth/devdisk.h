struct dev_disk *dev_disk_create(char *file_name, unsigned int nblocks, bool_t sync);
void dev_disk_write(struct dev_disk *dd, unsigned int offset, const char *data,
				void (*completion)(void *arg, bool_t success), void *arg);
void dev_disk_read(struct dev_disk *dd, unsigned int offset, char *data,
				void (*completion)(void *arg, bool_t success), void *arg);
