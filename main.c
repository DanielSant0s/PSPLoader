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
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <elf.h>

#include <ps2_filesystem_driver.h>

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

typedef struct {
    int type;
    char *name;
    Elf32_Addr virtual_addr;
    Elf32_Off section_vaddr;
    Elf32_Addr virtual_offset;
} SymbolEntry;

void print_symbols(SymbolEntry *symbols, int num_symbols) {
    for (int i = 0; i < num_symbols; i++) {
        printf("%s - %d - 0x%08lx - 0x%08lx - 0x%08lx\n",
               symbols[i].name,
               symbols[i].type,
               symbols[i].virtual_addr,
               symbols[i].virtual_offset,
               symbols[i].section_vaddr);
    }
}

void read_symbols(char *filename) {
    int fd = open(filename, O_RDONLY);

    Elf32_Ehdr elf_header;
    read(fd, &elf_header, sizeof(Elf32_Ehdr));

    Elf32_Shdr section_header;
    int shdr_size = elf_header.e_shentsize;
    int strtab_offset = 0;

    for (int i = 0; i < elf_header.e_shnum; i++) {
        lseek(fd, elf_header.e_shoff + (i * shdr_size), SEEK_SET);
        read(fd, &section_header, shdr_size);

        if (section_header.sh_type == SHT_SYMTAB) {
            int symbol_count = section_header.sh_size / sizeof(Elf32_Sym);
            Elf32_Sym *symbol_table = (Elf32_Sym *) malloc(section_header.sh_size);
            lseek(fd, section_header.sh_offset, SEEK_SET);
            read(fd, symbol_table, section_header.sh_size);

            int strtab_section_index = section_header.sh_link;
            Elf32_Shdr strtab_section_header;
            lseek(fd, elf_header.e_shoff + (strtab_section_index * shdr_size), SEEK_SET);
            read(fd, &strtab_section_header, shdr_size);

            char *strtab = (char *) malloc(strtab_section_header.sh_size);
            lseek(fd, strtab_section_header.sh_offset, SEEK_SET);
            read(fd, strtab, strtab_section_header.sh_size);

            SymbolEntry *symbols = (SymbolEntry *) malloc(symbol_count * sizeof(SymbolEntry));

            for (int j = 0; j < symbol_count; j++) {
                Elf32_Sym symbol = symbol_table[j];
                SymbolEntry entry;

                if (ELF32_ST_TYPE(symbol.st_info) == STT_FUNC) {
                    entry.type = 1;
                } else {
                    entry.type = 2;
                }

                entry.name = &strtab[symbol.st_name];
                entry.virtual_addr = symbol.st_value;
                entry.section_vaddr = section_header.sh_offset;
                entry.virtual_offset = section_header.sh_addr;

                symbols[j] = entry;
            }

            print_symbols(symbols, symbol_count);

            free(symbols);
            free(symbol_table);
            free(strtab);

            break;
        }
    }

    close(fd);
}

int main(int argc, char *argv[]) {
    Elf32_Ehdr elf_header;
    Elf32_Shdr shstr_header;
    char* section_names = NULL;
    int fd;

    reset_IOP();
	init_drivers();
    init_scr();

    // scr_setfontcolor(0xFFFF0080);
    // scr_setbgcolor(0xFF202020);

    scr_printf("PSP Executable loader PoC\n");
    scr_printf("Created by Daniel Santos\n");

    scr_printf("Loading PSP executable...\n\n");
    fd = open("hello.elf", O_RDONLY);
    read(fd, &elf_header, sizeof(Elf32_Ehdr));

    scr_printf("Total sections: %d\n", elf_header.e_shnum);
    scr_printf("Entrypoint: 0x%lx\n", elf_header.e_entry);

    scr_printf("Reading executable section info...");

    sleep(2);
    scr_clear();

    lseek(fd, elf_header.e_shoff + elf_header.e_shstrndx * sizeof(Elf32_Shdr), SEEK_SET);
    read(fd, &shstr_header, sizeof(Elf32_Shdr));

    section_names = malloc(shstr_header.sh_size);
    lseek(fd, shstr_header.sh_offset, SEEK_SET);
    read(fd, section_names, shstr_header.sh_size);

    Elf32_Shdr* section_header = malloc(sizeof(Elf32_Shdr)*elf_header.e_shnum);

    lseek(fd, elf_header.e_shoff, SEEK_SET);
    read(fd, section_header, sizeof(Elf32_Shdr)*elf_header.e_shnum);

    for(int j = 1; j < elf_header.e_shnum; j++
    ){
        scr_printf("%s section - address: 0x%lx | offset: 0x%lx | size: %ld\n", section_names + section_header[j].sh_name, section_header[j].sh_addr, section_header[j].sh_offset, section_header[j].sh_size);
    }

    free(section_header);
    free(section_names);

    close(fd);

    scr_clear();

    read_symbols("hello.elf");

    SleepThread();

    deinit_drivers();
}