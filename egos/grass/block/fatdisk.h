#ifdef HW_FS	

/* FAT File System Layout
 * +-------------+-----------------+---------------+---------------+-----+-------------------+
 * | super block |   inode blocks  |   fat blocks  | data cluster1 | ... | last data cluster |
 * +-------------+-----------------+---------------+---------------+-----+-------------------+
 * |<- 1 block ->|<-n_inodeblocks->|<-n_fatblocks->|
 */

typedef unsigned int fatentry_no;       // index of a fat entry


#define INODES_PER_BLOCK    (BLOCK_SIZE / sizeof(struct fatdisk_inode))
#define FAT_PER_BLOCK       (BLOCK_SIZE / sizeof(struct fatdisk_fatentry))

/* Contents of the "superblock".  There is only one of these.
 */
struct fatdisk_superblock {
    block_no n_inodeblocks;     // # blocks containing inodes
    block_no n_fatblocks;       // # blocks containing fat entries
    fatentry_no fat_free_list;       // fat index of the first free fat entry
};

/* An inode describes a file (= virtual block store).  "nclusters" contains
 * the number of clusters in the file, while "head" is the first fat
 * entry for the file.  Note that initially "all files exist" but are of
 * length 0.  It is intended that keeping track which files are free or
 * not is maintained elsewhere.
 */
struct fatdisk_inode {
    fatentry_no head;         // first fat entry
    block_no nblocks;     // total size (in clusters) of the file
};

struct fatdisk_fatentry {
    fatentry_no next;           // next entry in the file or in the free list
                                // 0 for EOF or end of free list
};

/* An inode block is filled with inodes.
 */
struct fatdisk_inodeblock {
    struct fatdisk_inode inodes[INODES_PER_BLOCK];
};

/* An fat block is filled with fatentries
 */
struct fatdisk_fatblock {
    struct fatdisk_fatentry entries[FAT_PER_BLOCK];
};

/* A convenient structure that's the union of all block types.  It should
 * have size BLOCK_SIZE, which may not be true for the elements.
 */
union fatdisk_block {
    block_t datablock;
    struct fatdisk_superblock superblock;
    struct fatdisk_inodeblock inodeblock;
    struct fatdisk_fatblock fatblock;
};

#endif
