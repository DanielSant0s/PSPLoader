#include <tamtypes.h>
#include <kernel.h>
#include <sifrpc.h>
#include <loadfile.h>
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <iopcontrol.h>
#include <sbv_patches.h>
#include <stdbool.h>
#include <debug.h>

#include <ps2_filesystem_driver.h>

typedef struct
{
	u8 ident[16];  // struct definition for ELF object header
	u16 type;
	u16 machine;
	u32 version;
	u32 entry;
	u32 phoff;
	u32 shoff;
	u32 flags;
	u16 ehsize;
	u16 phentsize;
	u16 phnum;
	u16 shentsize;
	u16 shnum;
	u16 shstrndx;
} elf_header_t;

typedef struct
{
     u32 sh_name;
     u32 sh_type;
     u32 sh_flags;
     u32 sh_addr;
     u32 sh_offset;
     u32 sh_size;
     u32 sh_link;
     u32 sh_info;
     u32 sh_addralign;
     u32 sh_entsize;
} section_header_t;

static void reset_IOP() {
	SifInitRpc(0);
	#if !defined(DEBUG) || defined(BUILD_FOR_PCSX2)
	/* Comment this line if you don't wanna debug the output */
	while(!SifIopReset(NULL, 0)){};
	#endif

	while(!SifIopSync()){};
	SifInitRpc(0);
	sbv_patch_enable_lmb();
	sbv_patch_disable_prefix_check();
}

static void init_drivers() {
	init_ps2_filesystem_driver();
}

static void deinit_drivers() {
	deinit_ps2_filesystem_driver();
}

int main(int argc, char *argv[]) {
    reset_IOP();
	init_drivers();
    init_scr();

    scr_printf("PSP Executable loader PoC\n");
    scr_printf("Created by Daniel Santos\n");

    SleepThread();

    deinit_drivers();
}