#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <mach-o/loader.h>
#include <stdio.h>

#if defined(MAKE_ARM64)

// XXX didn't work out of box and I'm tired after almost 20 hours to debug

#define ARM_THREAD_STATE64		6

typedef uint32_t __uint32_t;
typedef uint64_t __uint64_t;

#define _STRUCT_ARM_THREAD_STATE64	struct __darwin_arm_thread_state64
_STRUCT_ARM_THREAD_STATE64
{
	__uint64_t    __x[29];	/* General purpose registers x0-x28 */
	__uint64_t    __fp;		/* Frame pointer x29 */
	__uint64_t    __lr;		/* Link register x30 */
	__uint64_t    __sp;		/* Stack pointer x31 */
	__uint64_t    __pc;		/* Program counter */
	__uint32_t    __cpsr;	/* Current program status register */
	__uint32_t    __pad;    /* Same size for 32-bit or 64-bit clients */
};

typedef _STRUCT_ARM_THREAD_STATE64		arm_thread_state64_t;
#define ARM_THREAD_STATE64_COUNT ((mach_msg_type_number_t) \
   (sizeof (arm_thread_state64_t)/sizeof(uint32_t)))

#endif

#define roundx(x, y) ((x & ~(y-1)) + ((x & (y-1)) ? y : 0))

#if !defined(LINKEDIT) && !defined(FAKELINKEDIT) && defined(DYLD)
#warning dyld needs __LINKEDIT
#endif

#if defined(FUNCTION_STARTS) && !defined(LINKEDIT)
#error FUNCTION_STARTS requires LINKEDIT
#endif

#if defined(LINKEDIT) && defined(FAKELINKEDIT)
#error LINKEDIT and FAKELINKEDIT are incompatible
#endif

#if !defined(THREAD_CMD) && !defined(ENTRYP_CMD)
#warning Specify either THREAD_CMD or ENTRYP_CMD
#endif

#if defined(ENTRYP_CMD) && !defined(DYLD)
#warning LC_MAIN doesnt work without DYLINKER
// xnu-4570.1.46/bsd/kern/mach_loader.c
// 1977  	/* kernel does *not* use entryoff from LC_MAIN.	 Dyld uses it. */
// 1978  	result->needs_dynlinker = TRUE;
// 1979  	result->using_lcmain = TRUE;
#endif

#if defined(DYLD) && !defined(DYLIB)
#warning dyld needs "linking" to libdyld.dylib
#endif

#if defined(DYLD) && !(defined(DYLDINFO) && defined(SYMTAB) && defined(DYSYMTAB))
#warning DYLD needs at least empty DYLDINFO, SYMTAB and DYSYMTAB
#endif

#if !defined(MAKE_X86_64) && !defined(MAKE_X86_32) && !defined(MAKE_ARM64)
#error Specify MAKE_X86_64 or MAKE_X86_32 or MAKE_ARM64
#endif

#if defined(MAKE_X86_64) || defined(MAKE_ARM64)
#define MAKE_64 1
#else
#define MAKE_64 0
#endif

#ifdef NOPAD
#warning No padding results in what kernel considers malformed mach-o. (thanks, TaiG? :)
#endif

#if !defined(PAGEZERO) && !defined(MAKE_X86_32)
#warning PAGEZERO is required everywhere except x86_32
#endif

#define MAXOSXVERSION 0x0B0700 // 11.07.0
#define LINKEDIT_SIZE 0x80
#define DYLINKER "/usr/lib/dyld"
#define LSYSTEM "/usr/lib/system/libdyld.dylib"
#define OUTF "a.out"
#define TEXTF "__TEXT.__text"

#if MAKE_64
#define MH_MAGIC_X MH_MAGIC_64
typedef struct mach_header_64 mach_header_X;
#define LC_SEGMENT_X LC_SEGMENT_64
typedef struct segment_command_64 segment_command_X;
typedef struct section_64 section_X;
#else
#define MH_MAGIC_X MH_MAGIC
typedef struct mach_header mach_header_X;
#define LC_SEGMENT_X LC_SEGMENT
typedef struct segment_command segment_command_X;
typedef struct section section_X;
#endif

#if defined(MAKE_ARM64)
#define PAGE_SIZE_X 0x8000
#elif defined(MAKE_X86_32) || defined(MAKE_X86_64)
#define PAGE_SIZE_X 0x1000
#else
#error Cant determite PAGE_SIZE_X
#endif

int main(void) {
	int aoutfd = open(OUTF, O_CREAT | O_TRUNC | O_WRONLY);
	chmod(OUTF, 0755);
	
	size_t text_size;
	uint8_t* text_dat;

	{
		int fd = open(TEXTF, O_RDONLY);
		struct stat st;
		fstat(fd, &st);
		text_size = st.st_size;
		text_dat = malloc(text_size);
		read(fd, text_dat, text_size);
		close(fd);
	}

	mach_header_X header;
	{
		// fix sizeofcmds and nsects
		bzero(&header, sizeof(header));
		header.magic = MH_MAGIC_X;
		header.filetype = MH_EXECUTE;
#ifdef MAKE_X86_64
		header.cputype = CPU_TYPE_X86_64;
		header.cpusubtype = 3;
#elif MAKE_X86_32
		header.cputype = CPU_TYPE_X86;
		header.cpusubtype = 3;
#elif MAKE_ARM64
		header.cputype = CPU_TYPE_ARM64;
		header.cpusubtype = 0;
#else
#error Unsupported cputype/cpusubtype
#endif
		header.flags = MH_PIE;
#ifdef DYLD
		header.flags |= MH_TWOLEVEL | MH_DYLDLINK | MH_NOUNDEFS;
#endif
#if !MAKE_64
		header.flags |= MH_NO_HEAP_EXECUTION;
#endif
	}

#ifdef PAGEZERO
	segment_command_X pagezero;
	{
		bzero(&pagezero, sizeof(pagezero));
		pagezero.cmd = LC_SEGMENT_X;
		pagezero.cmdsize = sizeof(pagezero);
		strcpy(pagezero.segname, SEG_PAGEZERO);
		pagezero.vmaddr = 0;
#ifdef MAKE_ARM64
		pagezero.vmsize = 0x100000000;
#else
		pagezero.vmsize = PAGE_SIZE_X;
#endif
		pagezero.fileoff = 0;
		pagezero.filesize = 0;
		pagezero.maxprot = VM_PROT_NONE;
		pagezero.initprot = VM_PROT_NONE;
		pagezero.nsects = 0;
		pagezero.flags = 0;
		++header.ncmds;
		header.sizeofcmds += pagezero.cmdsize;
	}
#endif

	segment_command_X text_seg;
	{
		// fix cmdsize and header.sizeofcmds
		// fix text_seg.filesize
		bzero(&text_seg, sizeof(text_seg));
		text_seg.cmd = LC_SEGMENT_X;
		text_seg.cmdsize = sizeof(text_seg);

		strcpy(text_seg.segname, SEG_TEXT);
#ifdef PAGEZERO
		text_seg.vmaddr = pagezero.vmaddr + pagezero.vmsize;
#else
		text_seg.vmaddr = 0x1000;
#endif
		// XXX roundx(text_dat + sizeof(header) + header.sizeofcmds, PAGE_SIZE_X)
		text_seg.vmsize = PAGE_SIZE_X;
		text_seg.fileoff = 0;
		text_seg.maxprot = VM_PROT_READ | VM_PROT_WRITE | VM_PROT_EXECUTE;
		text_seg.initprot = VM_PROT_READ | VM_PROT_EXECUTE;
#ifdef TEXT_SECT
		text_seg.nsects = 1;
#else
		text_seg.nsects = 0;
#endif
		text_seg.flags = 0;
		++header.ncmds;
		header.sizeofcmds += text_seg.cmdsize;
	}

#ifdef TEXT_SECT
	section_X text_sect;
	{
		// fix text_sect.addr
		// fix text_sect.offset
		// fix text_sect.size
		bzero(&text_sect, sizeof(text_sect));
		strcpy(text_sect.sectname, SECT_TEXT);
		strcpy(text_sect.segname, SEG_TEXT);
		text_sect.align = 0;
		text_sect.reloff = 0;
		text_sect.nreloc = 0;
		text_sect.flags = S_ATTR_PURE_INSTRUCTIONS|S_ATTR_SOME_INSTRUCTIONS;
		
		// fixing cmdsize and header.sizeofcmds
		text_seg.cmdsize += sizeof(text_sect);
		header.sizeofcmds += sizeof(text_sect);
	}
#endif

#if defined(LINKEDIT) || defined(FAKELINKEDIT)
	segment_command_X linkedit_seg;
	{
		// fix linkedit_seg.filesize
		// fix linkedit_seg.fileoff
		// fix linkedit_seg.vmsize
		bzero(&linkedit_seg, sizeof(linkedit_seg));
		linkedit_seg.cmd = LC_SEGMENT_X;
		linkedit_seg.cmdsize = sizeof(linkedit_seg);

		strcpy(linkedit_seg.segname, SEG_LINKEDIT);
		linkedit_seg.vmaddr = text_seg.vmaddr + text_seg.vmsize;
		linkedit_seg.maxprot = VM_PROT_READ | VM_PROT_WRITE | VM_PROT_EXECUTE;
		linkedit_seg.initprot = VM_PROT_READ | VM_PROT_EXECUTE;
		linkedit_seg.nsects = 0;
		linkedit_seg.flags = 0;
		
		++header.ncmds;
		header.sizeofcmds += linkedit_seg.cmdsize;
	}
#endif

#ifdef SYMTAB
	struct symtab_command symtab;
	{
		// fix symoff
		// fix stroff
		bzero(&symtab, sizeof(symtab));
		symtab.cmd = LC_SYMTAB;
		symtab.cmdsize = sizeof(symtab);

		++header.ncmds;
		header.sizeofcmds += symtab.cmdsize;
	}
#endif

#ifdef DYSYMTAB
	struct dysymtab_command dysymtab;
	{
		bzero(&dysymtab, sizeof(dysymtab));
		dysymtab.cmd = LC_DYSYMTAB;
		dysymtab.cmdsize = sizeof(dysymtab);

		++header.ncmds;
		header.sizeofcmds += dysymtab.cmdsize;
	}
#endif

#ifdef DYLD
	size_t dyld_size = sizeof(struct dylinker_command) + strlen(DYLINKER);
#ifndef NO_CMDSIZE_ROUND
	dyld_size = roundx(dyld_size, 8);
#endif
	struct dylinker_command *dyld_cmd = __builtin_alloca(dyld_size);
	{
		bzero(dyld_cmd, dyld_size);
		dyld_cmd->cmd = LC_LOAD_DYLINKER;
		dyld_cmd->cmdsize = dyld_size;
		dyld_cmd->name.offset = sizeof(struct dylinker_command);
		memcpy(((char*)dyld_cmd) + dyld_cmd->name.offset, DYLINKER, strlen(DYLINKER));

		++header.ncmds;
		header.sizeofcmds += dyld_cmd->cmdsize;
	}
#endif

#ifdef DYLDINFO
	// dyld fails if at least emty LC_DYLD_INFO_ONLY command isn't present
	struct dyld_info_command dyldinfo_cmd;
	{
		bzero(&dyldinfo_cmd, sizeof(dyldinfo_cmd));
		dyldinfo_cmd.cmd = LC_DYLD_INFO_ONLY;
		dyldinfo_cmd.cmdsize = sizeof(dyldinfo_cmd);

		++header.ncmds;
		header.sizeofcmds += dyldinfo_cmd.cmdsize;
	}
#endif

#ifdef DYLIB
	size_t dylib_size = sizeof(struct dylib_command) + strlen(LSYSTEM);
#ifndef NO_CMDSIZE_ROUND
	dylib_size = roundx(dylib_size, 8);
#endif
	struct dylib_command *dylib_cmd = __builtin_alloca(dylib_size);
	{
		bzero(dylib_cmd, dylib_size);
		dylib_cmd->cmd = LC_LOAD_DYLIB;
		dylib_cmd->cmdsize = dylib_size;
		dylib_cmd->dylib.name.offset = sizeof(struct dylib_command);
		memcpy(((char*)dylib_cmd) + dylib_cmd->dylib.name.offset, LSYSTEM, strlen(LSYSTEM));

		++header.ncmds;
		header.sizeofcmds += dylib_cmd->cmdsize;
	}
#endif

#ifdef VERSION
	struct version_min_command version_cmd;
	{
		bzero(&version_cmd, sizeof(version_cmd));
		version_cmd.cmd = LC_VERSION_MIN_MACOSX;
		version_cmd.cmdsize = sizeof(version_cmd);
		version_cmd.version = MAXOSXVERSION;

		++header.ncmds;
		header.sizeofcmds += version_cmd.cmdsize;
	}
#endif

#ifdef THREAD_CMD
#if defined(MAKE_X86_64)
#define set_thc_entryp(thc, val) (thc.state.__rip = val);
#elif defined(MAKE_X86_32)
#define set_thc_entryp(thc, val) (thc.state.__eip = val);
#elif defined(MAKE_ARM64)
#define set_thc_entryp(thc, val) (thc.state.__pc = val);
#else
#error no thread for this type
#endif

	struct thread_command {
		uint32_t	cmd;		/* LC_THREAD or  LC_UNIXTHREAD */
		uint32_t	cmdsize;	/* total size of this command */
		x86_state_hdr_t hdr;
#if defined(MAKE_X86_64)
		x86_thread_state64_t state;
#elif defined(MAKE_X86_32)
		x86_thread_state32_t state;
#elif defined(MAKE_ARM64)
		arm_thread_state64_t state;
#endif
	} thread_command;

	{
		// fix via set_thc_entryp
		bzero(&thread_command, sizeof(thread_command));
		thread_command.cmd = LC_UNIXTHREAD;
		thread_command.cmdsize = sizeof(thread_command);
#if defined(MAKE_X86_64)
		thread_command.hdr.flavor = x86_THREAD_STATE64;
#elif defined(MAKE_X86_32)
		thread_command.hdr.flavor = x86_THREAD_STATE32;
#elif defined(MAKE_ARM64)
		thread_command.hdr.flavor = ARM_THREAD_STATE64;
#endif
		thread_command.hdr.count = sizeof(thread_command.state)/sizeof(uint32_t);
	}
	++header.ncmds;
	header.sizeofcmds += thread_command.cmdsize;
#endif

#ifdef ENTRYP_CMD
	struct entry_point_command entryp_cmd;
	{
		bzero(&entryp_cmd, sizeof(entryp_cmd));
		entryp_cmd.cmd = LC_MAIN;
		entryp_cmd.cmdsize = sizeof(entryp_cmd);
		entryp_cmd.entryoff = 0;
		entryp_cmd.stacksize = 0;

		++header.ncmds;
		header.sizeofcmds += entryp_cmd.cmdsize;
	}
#endif

#ifdef LINKEDIT
	size_t linkedit_size = LINKEDIT_SIZE;
	size_t linkedit_curr_off = 0;
	uint8_t *linkedit_dat = __builtin_alloca(linkedit_size);
	{
		bzero(linkedit_dat, linkedit_size);
	}
#endif
	
#ifdef FUNCTION_STARTS
	struct linkedit_data_command function_starts_cmd;
	{
		bzero(&function_starts_cmd, sizeof(function_starts_cmd));
		function_starts_cmd.cmd = LC_FUNCTION_STARTS;
		function_starts_cmd.cmdsize = sizeof(function_starts_cmd);
		
		function_starts_cmd.dataoff = linkedit_curr_off;
		function_starts_cmd.datasize = 8;
		linkedit_curr_off += function_starts_cmd.datasize;

		++header.ncmds;
		header.sizeofcmds += sizeof(function_starts_cmd);
	}
#endif

	// header
	write(aoutfd, &header, sizeof(header));

#ifdef PAGEZERO
	// pagezero
	write(aoutfd, &pagezero, sizeof(pagezero));
#endif

	// text segment
	text_seg.filesize = sizeof(header) + header.sizeofcmds + text_size;
#ifndef NOPAD
	text_seg.filesize = roundx(text_seg.filesize, PAGE_SIZE_X);
#endif

	write(aoutfd, &text_seg, sizeof(text_seg));

#ifdef TEXT_SECT
	// text section
	text_sect.offset = sizeof(header) + header.sizeofcmds;
	text_sect.addr = text_seg.vmaddr + text_sect.offset;
	text_sect.size = text_seg.vmsize - text_sect.offset;
	write(aoutfd, &text_sect, sizeof(text_sect));
#endif

#ifdef LINKEDIT
	// linkedit segment
	linkedit_seg.filesize = linkedit_size;
#ifdef TEXT_SECT
	linkedit_seg.fileoff = text_sect.offset + text_sect.size;
#else
	linkedit_seg.fileoff = text_seg.vmsize;
#endif
	linkedit_seg.vmsize = linkedit_size;
	write(aoutfd, &linkedit_seg, sizeof(linkedit_seg));
#endif

#ifdef FAKELINKEDIT
	linkedit_seg.fileoff = text_seg.fileoff + text_seg.filesize;
	write(aoutfd, &linkedit_seg, sizeof(linkedit_seg));
#endif

#ifdef SYMTAB
	// symtab
#if defined(LINKEDIT) || defined(FAKELINKEDIT)
	symtab.symoff = linkedit_seg.fileoff;
#else
	symtab.symoff = text_seg.fileoff;
#endif
	symtab.stroff = symtab.symoff;
	write(aoutfd, &symtab, symtab.cmdsize);
#endif

#ifdef DYSYMTAB
	// dysymtab
	write(aoutfd, &dysymtab, dysymtab.cmdsize);
#endif

#ifdef DYLD
	// dyld
	write(aoutfd, dyld_cmd, dyld_size);
#endif

#ifdef DYLDINFO
	// dyldinfo
	write(aoutfd, &dyldinfo_cmd, dyldinfo_cmd.cmdsize);
#endif

#ifdef DYLIB
	write(aoutfd, dylib_cmd, dylib_size);
#endif

#ifdef VERSION
	// version
	write(aoutfd, &version_cmd, version_cmd.cmdsize);
#endif

#if defined(TEXT_ALIGN_END) && defined(TEXT_SECT)
	{
		size_t diff = text_sect.size - text_size;
		text_sect.addr += diff;
	}
#endif

#ifdef TEXT_SECT
	uint64_t entryp = text_sect.addr;
#elif defined(TEXT_ALIGN_END)
	uint64_t entryp = text_seg.vmaddr + sizeof(header) + header.sizeofcmds;
	{
		size_t diff = (text_seg.vmsize - (sizeof(header) + header.sizeofcmds)) - text_size;
		entryp += diff;
	}
#else
	uint64_t entryp = text_seg.vmaddr + sizeof(header) + header.sizeofcmds;	
#endif

#ifdef THREAD_CMD
	// thread
	set_thc_entryp(thread_command, entryp);
	write(aoutfd, &thread_command, thread_command.cmdsize);
#endif

#ifdef ENTRYP_CMD
	entryp_cmd.entryoff = entryp - text_seg.vmaddr;
	write(aoutfd, &entryp_cmd, entryp_cmd.cmdsize);
#endif

#ifdef FUNCTION_STARTS
	// function starts
	function_starts_cmd.dataoff += linkedit_seg.fileoff;
	write(aoutfd, &function_starts_cmd, function_starts_cmd.cmdsize);
#endif

#ifndef TEXT_ALIGN_END
	write(aoutfd, text_dat, text_size);
#endif

#if !defined(NOPAD) && (defined(LINKEDIT) || defined(TEXT_ALIGN_END))
	{
#ifdef TEXT_SECT
		size_t expected = text_sect.size;
#else
		size_t expected = text_seg.vmsize - sizeof(header) - header.sizeofcmds;
#endif
		if (text_size < expected) {
			size_t diff = expected - text_size;
			char* pad = malloc(diff);
			for (int i = 0; i < diff; i += sizeof(uint64_t)) {
				*(uint64_t*)(pad + i) = 0x00feed00dead00f00d;
			}
			// leftover (diff - i + sizeof(uint64_t))
			write(aoutfd, pad, diff);
			free(pad);
		}
	}
#endif

#ifdef TEXT_ALIGN_END
	write(aoutfd, text_dat, text_size);
#endif

#ifdef FUNCTION_STARTS
	*(uint64_t*)(linkedit_dat + function_starts_cmd.dataoff - linkedit_seg.fileoff) = entryp;
#endif

#ifdef LINKEDIT
	write(aoutfd, linkedit_dat, linkedit_size);
#endif
	
#ifndef NOPAD
	{
		size_t pos = lseek(aoutfd, 0, SEEK_CUR);
		size_t expected = text_seg.filesize;
#ifdef LINKEDIT 
		expected += linkedit_seg.filesize;
#endif

		if (pos < expected) {
			size_t diff = expected - pos;
			char* pad = malloc(diff);
			for (int i = 0; i < diff; i += sizeof(uint64_t)) {
				*(uint64_t*)(pad + i) = 0x00feed00dead00f00d;
			}
			// leftover (diff - i + sizeof(uint64_t))
			write(aoutfd, pad, diff);
			free(pad);
		}
	}
#endif

	free(text_dat);
	close(aoutfd);
}