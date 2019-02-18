SNAME := $(shell ./sname)

GRASS_DIR = .
include Makefile.$(SNAME)

CFLAGS = -c -g $(XFLAGS) -DGRASS -D_XOPEN_SOURCE -Wall -Wsign-compare -DHW_MLFQ -DHW_MEASURE -DHW_PAGING

EARTH_SRCS = earth/clock.c earth/devdisk.c earth/devtty.c earth/devudp.c earth/intr.c earth/log.c earth/mem.c earth/myalloc.c earth/prot.c earth/tlb.c
GRASS_SRCS = grass/blocksvr.c grass/dirsvr.c grass/disksvr.c grass/blkfilesvr.c grass/main.c grass/process.c grass/procsys.c grass/ramfilesvr.c grass/spawnsvr.c grass/ttysvr.c
BLOCK_SRCS = grass/block/clockdisk.c grass/block/fatdisk.c grass/block/partdisk.c grass/block/protdisk.c grass/block/raid0disk.c grass/block/raid1disk.c grass/block/ramdisk.c grass/block/treedisk.c grass/block/checkdisk.c
SHARED_SRCS = shared/block.c shared/dir.c shared/ema.c shared/file.c shared/queue.c shared/spawn.c
KERNEL_SRCS = $(EARTH_SRCS) $(GRASS_SRCS) $(BLOCK_SRCS)
CSRCS = $(KERNEL_SRCS) $(SHARED_SRCS)

OBJS = $(KERNEL_SRCS:.c=.o) $(SHARED_SRCS:shared/%.c=grass/%.o) $(ASM_SRCS:.s=.o)

all: a.out
	make -f Makefile.apps

a.out: $(OBJS)
	$(CC) $(OBJS) -lm

grass/%.o: shared/%.c
	$(CC) -Ishared -Igrass $(CFLAGS) $< -o $@

run: all
	@echo '============================'
	@echo '>>> hit <ctrl>q to exit  <<<'
	@echo '>>> hit <ctrl>l for dump <<<'
	@echo '============================'
	stty raw -echo; ./a.out; stty cooked echo

run2: all
	@echo '============================'
	@echo '>>> hit <ctrl>q to exit  <<<'
	@echo '>>> hit <ctrl>l for dump <<<'
	@echo '============================'
	rm -f disk.dev
	stty raw -echo; ./a.out; stty cooked echo

cpr: tools/cpr.c
	gcc -o cpr tools/cpr.c

clean:
	rm -f *.o */*.o */*/*.o *.d */*.d */*/*.d *.exe */*.exe *.int *.tmp a.out crt0.s macho_cvt elf_cvt libgrass.a log.txt disk.dev cpr
	find . -name '*.log' -exec rm -f '{}' ';'
	find . -name '*.aux' -exec rm -f '{}' ';'
	find . -name 'paper.pdf' -exec rm -f '{}' ';'

-include $(CSRCS:.c=.d)
