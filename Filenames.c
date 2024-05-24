/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Mission filenames
 *  Copyright (C) 2020 Christopher Bazley
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

#include <string.h>
#include <limits.h>

#include "Debug.h"
#include "Macros.h"
#include "Reader.h"
#include "Writer.h"

#include "SFError.h"
#include "Filenames.h"
#include "FilenamesData.h"
#include "DataType.h"

enum {
  FilenameScrambleRange = 256,
  FilenameScramblePasses = 10
};

static SFError read_filename(char *const filename,
  Reader *const reader, int *const scrambler)
{
  assert(filename);
  assert(scrambler);

  size_t nchars;
  int scrambler2 = *scrambler;
  for (nchars = 0; nchars < BytesPerFilename; ++nchars, --scrambler2)
  {
    int c = reader_fgetc(reader);
    if (c == EOF) {
      return SFERROR(ReadFail);
    }

    int scrambler3 = scrambler2;
    for (int pass = 0; pass < FilenameScramblePasses; ++pass)
    {
      if (scrambler3 < 0) {
        scrambler3 += FilenameScrambleRange;
      }
      assert(scrambler3 >= 0);
      assert(scrambler3 < FilenameScrambleRange);
      c ^= scrambler3;
      scrambler3 -= BytesPerFilename * DataType_FilenamesCount;
    }
    if (c == '\r') {
      c = '\0';
    }
    filename[nchars] = c;
    if (c == '\0') {
      break;
    }
  }

  if (nchars == BytesPerFilename) {
    return SFERROR(FilenameTooLong);
  }

  *scrambler -= BytesPerFilename;
  assert(nchars <= LONG_MAX);
  long int const padding = BytesPerFilename - (long)nchars - 1;
  if (reader_fseek(reader, padding, SEEK_CUR))
  {
    return SFERROR(BadSeek);
  }

  DEBUGF("Finished reading %zu-character filename, '%s', at %ld\n",
         nchars, filename, reader_ftell(reader));
  return SFERROR(OK);
}

SFError filenames_read(FilenamesData *const filenames,
  Reader *const reader)
{
  assert(filenames);
  int scrambler = -1;
  SFError err = SFERROR(OK);

  for (DataType data_type = DataType_First;
       (data_type < DataType_FilenamesCount) && !SFError_fail(err);
       ++data_type)
  {
    err = read_filename(filenames->names[data_type], reader, &scrambler);
  }

  DEBUGF("Finished reading filenames data at %ld\n", reader_ftell(reader));
  return err;
}

static void write_filename(char const *const filename,
  Writer *const writer, int *const scrambler)
{
  assert(filename);
  assert(scrambler);

  size_t nchars;
  int scrambler2 = *scrambler;
  for (nchars = 0; nchars < BytesPerFilename; ++nchars, --scrambler2)
  {
    int c = filename[nchars];
    if (c == '\0') {
      c = '\r';
    }

    int scrambler3 = scrambler2;
    int sc = c;
    for (int pass = 0; pass < FilenameScramblePasses; ++pass)
    {
      if (scrambler3 < 0) {
        scrambler3 += FilenameScrambleRange;
      }
      assert(scrambler3 >= 0);
      assert(scrambler3 < FilenameScrambleRange);
      sc ^= scrambler3;
      scrambler3 -= BytesPerFilename * DataType_FilenamesCount;
    }
    if (writer_fputc(sc, writer) == EOF) {
      return;
    }
    if (c == '\r') {
      break;
    }
  }
  assert(nchars < BytesPerFilename);

  *scrambler -= BytesPerFilename;
  assert(nchars <= LONG_MAX);
  long int const padding = BytesPerFilename - (long)nchars - 1;
  writer_fseek(writer, padding, SEEK_CUR);

  DEBUGF("Finished writing %zu-character filename, '%s', at %ld\n",
         nchars, filename, writer_ftell(writer));
}

void filenames_write(FilenamesData const *const filenames,
  Writer *const writer)
{
  assert(filenames);
  int scrambler = -1;

  for (DataType data_type = DataType_First;
       data_type < DataType_FilenamesCount;
       ++data_type)
  {
    write_filename(filenames->names[data_type], writer, &scrambler);
  }

  DEBUGF("Finished writing filenames data at %ld\n", writer_ftell(writer));
}

char const *filenames_get(FilenamesData const *const filenames, DataType const data_type)
{
  assert(filenames);
  assert(data_type >= 0);
  assert(data_type < ARRAY_SIZE(filenames->names));
  return filenames->names[data_type];
}

void filenames_set(FilenamesData *const filenames, DataType const data_type, char const *const name)
{
  assert(filenames);
  assert(data_type >= 0);
  assert(data_type < ARRAY_SIZE(filenames->names));
  assert(name);
  assert(strlen(name) < BytesPerFilename);
  DEBUG("Updating filename %d (was '%s', now '%s')",
        data_type, filenames->names[data_type], name);
  STRCPY_SAFE(filenames->names[data_type], name);
}
