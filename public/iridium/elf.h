/* This file defines types and structures for the ELF file format
 *
 * Note: This file is not exhaustive, and currently only contains
 * the required definitions to boot the Iridium operating system.
 * In particular, this file only contains definitions for 64 bit
 * elf files for the x86_64 platform.
 */

#ifndef PUBLIC_IRIDIUM_ELF_H_
#define PUBLIC_IRIDIUM_ELF_H_

#include <stdint.h>

// Data types

typedef uint16_t Elf64_Half;

typedef uint32_t Elf64_Word;
typedef int32_t Elf64_Sword;

typedef uint64_t Elf64_Xword;
typedef	int64_t  Elf64_Sxword;

// Addresses
typedef uint64_t Elf64_Addr;

// File offsets
typedef uint64_t Elf64_Off;

// Section indices
typedef uint16_t Elf64_Section;


// Elf file header
// Appears at the start of every elf file

#define EI_NIDENT (16)


typedef struct Elf64_Ehdr {
    unsigned char	e_ident[EI_NIDENT]; // Magic id and other info
    Elf64_Half	    e_type;			// Object file type
    Elf64_Half	    e_machine;		// Architecture
    Elf64_Word	    e_version;		// File format version
    Elf64_Addr	    e_entry;		// Entry point virtual address
    Elf64_Off	    e_phoff;		// File offset of program header table
    Elf64_Off	    e_shoff;		// File offset of section header table
    Elf64_Word	    e_flags;		// Processor-specific flags
    Elf64_Half	    e_ehsize;		// Elf header size in bytes
    Elf64_Half	    e_phentsize;	// Program header table entry size
    Elf64_Half	    e_phnum;		// Program header table entry count
    Elf64_Half	    e_shentsize;	// Section header table entry size
    Elf64_Half	    e_shnum;		// Section header table entry count
    Elf64_Half      e_shstrndx;		// Section header string table index
} Elf64_Ehdr;

#define EI_MAG0	0       // e_ident index of ELFMAG0
#define ELFMAG0 0x7f	// Magic string byte 0
#define EI_MAG1	1       // e_ident index of ELFMAG1
#define ELFMAG1	'E'		// Magic string byte 1
#define EI_MAG2	2       // e_ident index of ELFMAG2
#define ELFMAG2	'L'		// Magic string byte 2
#define EI_MAG3	3       // e_ident index of ELFMAG3
#define ELFMAG3	'F'		// Magic string byte 3

// Magic string as a single unit
#define	ELFMAG	"\177ELF"
#define	SELFMAG	4

#define EI_CLASS	    4	// File class byte index
#define ELFCLASSNONE	0	// Invalid class
#define ELFCLASS32	    1	// 32-bit objects (Not usable in Iridium)
#define ELFCLASS64	    2   // 64-bit objects
#define ELFCLASSNUM	    3

#define EI_ABIVERSION	8   // ABI version index
#define EI_PAD		    9	// Start index of padding bytes

// Elf format version
#define EV_NONE     0
#define EV_CURRENT  1
#define EV_NUM      2

// Machine type
#define EM_X86_64	62	// AMD x86-64 architecture


typedef struct Elf64_Shdr {
    Elf64_Word	sh_name;
    Elf64_Word	sh_type;
    Elf64_Xword	sh_flags;
    Elf64_Addr	sh_addr;
    Elf64_Off	sh_offset;
    Elf64_Xword	sh_size;
    Elf64_Word	sh_link;
    Elf64_Word	sh_info;
    Elf64_Xword	sh_addralign;
    Elf64_Xword	sh_entsize;
} Elf64_Shdr;


typedef struct Elf64_Phdr {
    Elf64_Word	p_type;
    Elf64_Word	p_flags;
    Elf64_Off	p_offset;
    Elf64_Addr	p_vaddr;
    Elf64_Addr	p_paddr;
    Elf64_Xword	p_filesz;
    Elf64_Xword	p_memsz;
    Elf64_Xword	p_align;
} Elf64_Phdr;

// Valid values for p_type
#define	PT_NULL		0
#define PT_LOAD		1
#define PT_DYNAMIC	2
#define PT_INTERP	3
#define PT_NOTE		4
#define PT_SHLIB	5
#define PT_PHDR		6
#define PT_TLS		7
#define	PT_NUM		8   // Total number of defined types

// p_flags
#define PF_X		(1 << 0)	// Segment is executable
#define PF_W		(1 << 1)	// Segment is writable
#define PF_R		(1 << 2)	// Segment is readable


#endif // PUBLIC_IRIDIUM_ELF_H_
