/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Compressed file utilities
 *  Copyright (C) 2021 Christopher Bazley
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public Licence as published by
 *  the Free Software Foundation; either version 2 of the Licence, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public Licence for more details.
 *
 *  You should have received a copy of the GNU General Public Licence
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "stdlib.h"
#include "stdio.h"
#include <string.h>
#include <stdbool.h>
#include <assert.h>
#include <limits.h>

#include "err.h"
#include "msgtrans.h"
#include "debug.h"
#include "FopenCount.h"
#include "ReaderGKey.h"
#include "WriterGKey.h"
#include "WriterGKC.h"
#include "DFile.h"
#include "DFileUtils.h"
#include "Utils.h"
#include "FilePaths.h"
#include "PathTail.h"

enum {
  HistoryLog2 = 9,
  WorstBitsPerChar = 9,
};

char *get_leaf_name(DFile *const dfile)
{
  return pathtail(dfile_get_name(dfile), 1);
}

int get_compressed_size(DFile *const dfile)
{
  long int size = 0;
  Writer writer;
  if (!writer_gkc_init_with_min(&writer, HistoryLog2, dfile_get_min_size(dfile), &size))
  {
    report_error(SFERROR(NoMem), "", "");
    return 0;
  }

  dfile_write(dfile, &writer);
  if (writer_destroy(&writer) == -1L)
  {
    return 0;
  }

  assert(size >= 0);
  assert(size < INT_MAX);
  return (int)size;
}

SFError read_compressed(DFile *const dfile, Reader *const reader)
{
  assert(dfile);
  DEBUGF("Reading %p from compressed stream\n", (void *)dfile);

  SFError err = SFERROR(OK);
  Reader gkreader;
  if (!reader_gkey_init_from(&gkreader, HistoryLog2, reader)) {
    err = SFERROR(NoMem);
  } else {
    err = dfile_read(dfile, &gkreader);
    reader_destroy(&gkreader);
  }
  return err;
}

SFError load_compressed(DFile *const dfile, char const *const fname)
{
  assert(dfile);
  assert(fname);
  DEBUGF("Reading %p from compressed file %s\n", (void *)dfile, fname);

#ifdef FORTIFY
  Fortify_CheckAllMemory();
#endif

  SFError err = SFERROR(OpenInFail);
  FILE *const f = fopen_inc(fname, "rb");
  if (f)
  {
    Reader reader;
    if (!reader_gkey_init(&reader, HistoryLog2, f))
    {
      err = SFERROR(NoMem);
    }
    else
    {
      err = dfile_read(dfile, &reader);
      reader_destroy(&reader);
    }
    fclose_dec(f);
  }

#ifdef FORTIFY
  Fortify_CheckAllMemory();
#endif
  return err;
}

SFError write_compressed(DFile *const dfile, Writer *const writer)
{
  assert(dfile);
  DEBUGF("Writing %p as compressed stream\n", (void *)dfile);

  SFError err = SFERROR(OK);
  Writer gkwriter;
  if (!writer_gkey_init_from(&gkwriter, HistoryLog2, dfile_get_min_size(dfile), writer)) {
    err = SFERROR(NoMem);
  } else {
    dfile_write(dfile, &gkwriter);
    if (writer_destroy(&gkwriter) == -1L)
    {
      err = SFERROR(WriteFail);
    }
  }
  return err;
}

SFError save_compressed(DFile *const dfile, char *const fname)
{
  assert(dfile);
  assert(fname);
  DEBUGF("Writing %p as compressed file %s\n", (void *)dfile, fname);

#ifdef FORTIFY
  Fortify_CheckAllMemory();
#endif

  SFError err = SFERROR(OK);
  FILE *const f = fopen_inc(fname, "wb");
  if (!f)
  {
    err = SFERROR(OpenOutFail);
  }
  else
  {
    Writer writer;
    if (!writer_gkey_init(&writer, HistoryLog2, dfile_get_min_size(dfile), f))
    {
      err = SFERROR(NoMem);
      fclose_dec(f);
    }
    else
    {
      dfile_write(dfile, &writer);
      bool success = (writer_destroy(&writer) != -1L);
      if (fclose_dec(f))
      {
        success = false;
      }
      if (!success)
      {
        err = SFERROR(WriteFail);
      }
    }
  }

#ifdef FORTIFY
  Fortify_CheckAllMemory();
#endif

  return err;
}

/* ----------------------------------------------------------------------- */

int worst_compressed_size(DFile *const dfile)
{
  /* Worst-case estimate */
  int const orig_size = dfile_get_min_size(dfile);
  return (int)sizeof(int32_t) +
    ((orig_size * WorstBitsPerChar) / CHAR_BIT);
}
