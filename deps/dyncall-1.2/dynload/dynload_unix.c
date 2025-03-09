/*

 Package: dyncall
 Library: dynload
 File: dynload/dynload_unix.c
 Description:
 License:

   Copyright (c) 2007-2018 Daniel Adler <dadler@uni-goettingen.de>,
                           Tassilo Philipp <tphilipp@potion-studios.com>

   Permission to use, copy, modify, and distribute this software for any
   purpose with or without fee is hereby granted, provided that the above
   copyright notice and this permission notice appear in all copies.

   THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
   WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
   MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
   ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
   WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
   ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
   OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

*/


/*

  dynload_unix.c

  dynload module for .so (unix) and .dylib (mach-o darwin/OS X) files

*/


#include "dynload.h"
#include "../autovar/autovar_OS.h"

#include <string.h>

#if defined(__GLIBC__)
/* @@@ version check glibc more precisely... dl_iterate_phdr(): glibc ver >= 2.2.4*/
#  if (__GLIBC__ >= 2) && (__GLIBC_MINOR__ >= 3)
#    define DL_USE_GLIBC_ITER_PHDR
#  endif
/* to access dl_iterate_phdr(), and related w/ glibc */
#  ifndef _GNU_SOURCE
#  define _GNU_SOURCE
#  define __USE_GNU
#  endif
#endif

#include <dlfcn.h>


DLLib* dlLoadLibrary(const char* libPath)
{
  return (DLLib*)dlopen(libPath, RTLD_LAZY); //@@@ should use RTLD_LAZY, maybe?
}


void* dlFindSymbol(DLLib* pLib, const char* pSymbolName)
{
  return dlsym((void*)pLib, pSymbolName);
}


void dlFreeLibrary(DLLib* pLib)
{
  /* Check for NULL for cross-platform consistency. *BSD seems to do that in
  dlclose, Linux does not. POSIX states "if handle does not refer to an open
  object, dlclose() returns a non-zero value", which unfortunately sounds
  like it's not explicitly specified. */
  if(pLib)
    dlclose((void*)pLib);
}



/* for dlopen-based dlGetLibraryPath impls below, prefer RTLD_NOLOAD that
 * merely checks lib names */
#if defined(RTLD_NOLOAD)
#  define RTLD_LIGHTEST RTLD_LAZY|RTLD_NOLOAD
#else
#  define RTLD_LIGHTEST RTLD_LAZY
#endif


/* helper copying string if buffer big enough, returning length (without \0) */
static int dl_strlen_strcpy(char* dst, const char* src, int dstSize)
{
  int l = strlen(src);
  if(l < dstSize) /* l+'\0' <= bufSize */
    strcpy(dst, src);
  return l;
}

/* code for dlGetLibraryPath() is platform specific */

/* if dlinfo() exists use it (except on glibc, where it exists since version
 * 2.3.3, but its implementation is dangerous, as no checks are done whether
 * the handle is valid, thus rendering the returned values useless) check for
 * RTLD_DI_LINKMAP and RTLD_SELF, which are #defines used by dlinfo() on most
 * supported targets, or specifically check the OS (e.g. dlinfo() is originally
 * from Solaris) */
#if ((defined(RTLD_DI_LINKMAP) && defined(RTLD_SELF)) || defined(OS_SunOS)) && !defined(DL_USE_GLIBC_ITER_PHDR)

#include <link.h>

int dlGetLibraryPath(DLLib* pLib, char* sOut, int bufSize)
{
  struct link_map* p = NULL;
  int l = -1;
  if(dlinfo(pLib ? pLib : RTLD_SELF, RTLD_DI_LINKMAP, &p) == 0)
    l = dl_strlen_strcpy(sOut, p->l_name, bufSize);

  return l+1; /* strlen + '\0' */
}


/* specific implementation needed on Darwin -----> */
#elif defined(OS_Darwin)

#include <stdint.h>
#include <mach-o/dyld.h>

int dlGetLibraryPath(DLLib* pLib, char* sOut, int bufSize)
{
  uint32_t i;
  int l = -1;

  /* request info about own process? lookup first loaded image */
  if(pLib == NULL) {
    const char* libPath = _dyld_get_image_name(0); //@@@ consider using _NSGetExecutablePath()
    if(libPath)
      l = dl_strlen_strcpy(sOut, libPath, bufSize);
  }
  else {
    /* Darwin's code doesn't come with (non-standard) dlinfo(), so use dyld(1)
     * code. There doesn't seem to be a direct way to query the library path,
     * so "double-load" temporarily all already loaded images (just increases
     * ref count) and compare handles until we found ours. Return the name. */
    for(i=_dyld_image_count(); i>0;) /* backwards, ours is more likely at end */
    {
      const char* libPath = _dyld_get_image_name(--i);
      void* lib = dlopen(libPath, RTLD_LIGHTEST);
      if(lib) {
        dlclose(lib);

        /* compare handle pointers' high bits (in low 2 bits some flags might */
        /* be stored - should be safe b/c address needs alignment, anyways) */
        if(((uintptr_t)pLib ^ (uintptr_t)lib) < 4) {
          l = dl_strlen_strcpy(sOut, libPath, bufSize);
          break;
        }
      }
    }
  }

  return l+1; /* strlen + '\0' */
}


/* - OpenBSD >= 3.7 has dl_iterate_phdr(), as well as glibc >= 2.2.4
   - also some libc impls (like musl) provide dlinfo(), but not RTLD_SELF (see above), however they might come
     with dl_iterate_phdr (which comes from ELF program header iteration), so base it on that
   - skip and use dladdr()-based guessing (see below) if explicitly requested, e.g. by ./configure
   - Haiku/BeOS does have the headers but no implementation of dl_iterate_phdr() (at least as of 2021) */
#elif !defined(DL_DLADDR_TO_LIBPATH) && (defined(OS_OpenBSD) || defined(DL_USE_GLIBC_ITER_PHDR) || (!defined(RTLD_SELF) && defined(__ELF__))) && !defined(OS_BeOS)

#include <sys/types.h>
#include <link.h>

typedef struct {
  DLLib* pLib;
  char*  sOut;
  int    bufSize;
} iter_phdr_data;

static int iter_phdr_cb(struct dl_phdr_info* info, size_t size, void* data)
{
  int l = -1;
  iter_phdr_data* d = (iter_phdr_data*)data;
  void* lib = NULL;

  /* get loaded object's handle if not requesting info about process itself */
  if(d->pLib != NULL) {
    /* unable to relate info->dlpi_addr directly to our dlopen handle, let's
     * do what we do on macOS above, re-dlopen the already loaded lib (just
     * increases ref count) and compare handles */
    /* @@@ might be b/c it's the reloc addr... see below */
    lib = dlopen(info->dlpi_name, RTLD_LIGHTEST);
    if(lib)
      dlclose(lib);
  }

  /* compare handles and get name if found; if d->pLib == NULL this will
     enter info on first iterated object, which is the process itself */
  if(lib == (void*)d->pLib) {
    l = dl_strlen_strcpy(d->sOut, info->dlpi_name, d->bufSize);

    /* dlpi_name might be empty for the own process (d->pLib == NULL), so */
    /* try lookup via dladdr(proc_load_addr, ...) */
    if(l == 0 && d->pLib == NULL) {
      /* dlpi_addr is the reloc base (0 if PIE), find real virtual load addr */
      void* vladdr = (void*)info->dlpi_addr;
      int i = 0;
      for(; i < info->dlpi_phnum; ++i) {
        if(info->dlpi_phdr[i].p_type == PT_LOAD) {
          vladdr = (void*)(info->dlpi_addr + info->dlpi_phdr[i].p_vaddr);
          break;
        }
      }
      Dl_info di;
      if(dladdr(vladdr, &di) != 0)
        l = dl_strlen_strcpy(d->sOut, di.dli_fname, d->bufSize);
    }
  }

  return l+1; /* strlen + '\0'; is 0 if lib not found, which continues iter */
}

int dlGetLibraryPath(DLLib* pLib, char* sOut, int bufSize)
{
  iter_phdr_data d = { pLib, sOut, bufSize };
  return dl_iterate_phdr(iter_phdr_cb, &d);
}


/* glibc with neither dl_iterate_phdr() nor dlinfo() (latter introduced after former) @@@
#elif defined(__GLIBC__) && !defined(DL_USE_GLIBC_ITER_PHDR)

@@@impl */

/* fallback to dladdr() hack */
#else

#warning "Using non-optimal code for dlGetLibraryPath() b/c of platform limitations."

/* if nothing else is available, fall back to guessing using dladdr() - this */
/* might not always work, as it's trying to getit via the _fini() symbol,    */
/* which is usually defined in ELF files, but not guaranteed                 */

/* @@@Note: On some platforms this might be improved, e.g. on BeOS we have */
/* lt_dlgetinfo, which requires iterating over ltdl stuff, but was unable  */
/* to get that to work (would also introduce a link dependency on libltdl) */

int dlGetLibraryPath(DLLib* pLib, char* sOut, int bufSize)
{
/*@@@ missing handler for pLib == NULL*/
  /* cross fingers that shared object is standard ELF and look for _fini */
  int l = -1;
  void* s = dlsym((void*)pLib, "_fini");
  if(s) {
    Dl_info i;
    if(dladdr(s, &i) != 0)
      l = dl_strlen_strcpy(sOut, i.dli_fname, bufSize);
  }
  return l+1; /* strlen + '\0' */
}

#endif

