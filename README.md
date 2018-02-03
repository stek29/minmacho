# make.c usage
I was lazy to handle args properly, so it has to be recompiled each time.  
It creates executable mach-o file `a.out` which bundles code from file `__TEXT.__text`.  
Likely files larger than one page size are broken.

Supported options:
- Arch: MAKE_ARM64, MAKE_X86_32, MAKE_X86_64
- Entry point specifier: ENTRYP_CMD for LC_MAIN or THREAD_CMD for LC_UNIXTHREAD
- PAGEZERO -- to include a null mapping, needed everywhere except x86_32.
- LINKEDIT -- to include a `__LINKEDIT` segment
- FAKELINKEDIT -- same, but make it with vmsize=0 and filesize=0. Useful to get dyld working.
- DYLD -- include LC_LOAD_DYLINKER
- DYLDINO -- include LC_DYLD_INFO_ONLY
- SYMTAB -- include emtpy SYMTAB
- DYSYMTAB -- include empty DYSYMTAB
- DYLIB -- link to `/usr/lib/system/libdyld.dylib`, needed for `dyld` to get working.
- TEXT_SECT -- make a `__TEXT.__text` section.
- NOPAD -- don't add padding to make file at least of `PAGE_SIZE`. Useful for testing.
- NO_CMDSIZE_ROUND -- don't make cmdsize's divisible by 8 (for DYLD/DYLIB, where lc_str's are used).
- TEXT_ALIGN_END -- align code to the end -- useful for codesigning.
- FUNCTION_STARTS -- haven't really tested since it turened out to be useless
- VERSION -- macos version, not really tested

## ENTRYP_CMD & THREAD_CMD
LC_MAIN is handled by dyld.  
Therefore, `DYLD` is needed to make it work.

LC_UNIXTHREAD is handled by kernel itself, and can be used to set registers loaded when thread is started.

## DYLD
`dyld` would refuse to work if DYLDINO/SYMTAB/DYSYMTAB are missing, or if there's no (fake) LINKEDIT segment.  
And even with those `dyld` would fail with error `libdyld.dylib support not present for LC_MAIN`, because `libdyld.dylib` is simply not loaded.  
So that's why it needs `DYLIB`.

## SYMTAB
`dyld` doesn't like if `symtab.symoff` collides with mach-o header/commands, or if it's "under" LINKEDIT.

## PAGEZERO
All binaries are required to have `__PAGEZERO` (any mapping with VM_PROT_NONE protection covering page at 0x0).  
IIRC they (Apple) didn’t make it required on x86_32 mode only because some legacy softwaredidn’t have it and they’re afraid to break it.

## LINKEDIT & FAKELINKEDIT
LINKEDIT creates another segment with size 0x80.
FAKELINKEDIT creates another segment which is much like `__PAGEZERO` -- with 0 size. That's enough to get dyld happy.

## NOPAD
File size has to be at least of page size, or kernel would say it's "Bad Mach-O". Because of that, padding might be necessary. This option disables it.

## TEXT_ALIGN_END
Place actual code in the end of page instead of placing it right after the load commands. Needed for CodeSigning.

# main.c/main.s
I've made a couple of files printing "Hi Frand" and exiting with exit status 42 for different archs.
There's also a main.c for the reference.

# Smallest runnable Mach-O
See [post](https://stek29.rocks)

# License
MIT
