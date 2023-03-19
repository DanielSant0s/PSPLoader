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
    char *name;
    uint32_t addr;
    uint32_t offset;
    uint32_t size;
} SectionEntry;

typedef struct {
    SectionEntry* list;
    size_t count;
} Sections;

void print_sections(Sections *sections) {
    for (int i = 0; i < sections->count; i++) {
        scr_printf("%s - 0x%08lx - 0x%08lx - %ld\n",
               sections->list[i].name,
               sections->list[i].addr,
               sections->list[i].offset,
               sections->list[i].size);
    }
}

Sections* read_sections(const char* filename) {
    Sections* sec_info = malloc(sizeof(Sections));
    Elf32_Ehdr elf_header;
    Elf32_Shdr shstr_header;
    char* section_names = NULL;
    int fd;

    fd = open("hello.elf", O_RDONLY);
    read(fd, &elf_header, sizeof(Elf32_Ehdr));

    lseek(fd, elf_header.e_shoff + elf_header.e_shstrndx * sizeof(Elf32_Shdr), SEEK_SET);
    read(fd, &shstr_header, sizeof(Elf32_Shdr));

    section_names = malloc(shstr_header.sh_size);
    lseek(fd, shstr_header.sh_offset, SEEK_SET);
    read(fd, section_names, shstr_header.sh_size);

    Elf32_Shdr* section_header = malloc(sizeof(Elf32_Shdr)*elf_header.e_shnum);
    sec_info->list = malloc(sizeof(SectionEntry)*elf_header.e_shnum);

    lseek(fd, elf_header.e_shoff, SEEK_SET);
    read(fd, section_header, sizeof(Elf32_Shdr)*elf_header.e_shnum);

    for(int j = 0; j < elf_header.e_shnum; j++){
        sec_info->list[j].name = section_names + section_header[j].sh_name;
        sec_info->list[j].addr = section_header[j].sh_addr;
        sec_info->list[j].offset = section_header[j].sh_offset;
        sec_info->list[j].size = section_header[j].sh_size;
    }

    sec_info->count = elf_header.e_shnum;

    free(section_header);

    close(fd);

    return sec_info;
}

typedef struct {
    int type;
    char *name;
    uint32_t virtual_addr;
    uint32_t section_vaddr;
    uint32_t virtual_offset;
} SymbolEntry;

typedef struct {
    SymbolEntry* list;
    size_t count;
} Symbols;

void print_symbols(Symbols *symbols) {
    for (int i = 0; i < symbols->count; i++) {
        scr_printf("%s - %d - 0x%08lx - 0x%08lx - 0x%08lx\n",
               symbols->list[i].name,
               symbols->list[i].type,
               symbols->list[i].virtual_addr,
               symbols->list[i].section_vaddr,
               symbols->list[i].virtual_offset);
    }
}

Symbols* read_symbols(char *filename, Sections* section_table) {
    Symbols* symlist = NULL;
    int fd = open(filename, O_RDONLY);

    Elf32_Ehdr elf_header;
    read(fd, &elf_header, sizeof(Elf32_Ehdr));

    Elf32_Shdr section_header;
    int shdr_size = elf_header.e_shentsize;

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
                entry.section_vaddr = section_table->list[symbol.st_shndx].offset;
                entry.virtual_offset = section_table->list[symbol.st_shndx].addr;

                symbols[j] = entry;
            }

            symlist = malloc(sizeof(Symbols));
            symlist->list = symbols;
            symlist->count = symbol_count;

            free(symbol_table);
            free(strtab);

            break;
        }
    }

    close(fd);

    return symlist;
}

int main(int argc, char *argv[]) {
    reset_IOP();
	init_drivers();

    init_scr();
    scr_setCursor(0);

    // scr_setfontcolor(0xFFFF0080);
    // scr_setbgcolor(0xFF202020);

    scr_printf("PSP Executable loader PoC\n");
    scr_printf("Created by Daniel Santos\n");

    scr_printf("Loading PSP executable...\n\n");

    scr_clear();

    Sections* sections = read_sections("hello.elf");
    Symbols* syms = read_symbols("hello.elf", sections);

    print_sections(sections);
    print_symbols(syms);

    SleepThread();

    deinit_drivers();
}