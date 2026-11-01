/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  General utility functions
 *  Copyright (C) 2001 Christopher Bazley
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

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <limits.h>
#include <stdint.h>

#include "flex.h"
#include "Macros.h"
#include "Utils.h"

flex_ptr memset_flex(flex_ptr ptr, unsigned char c, size_t n)
{
  /* Version of memset() that doesn't need to be called with flex budge
  disabled */
  assert(flex_size(ptr) > 0); /* also validates anchor */
  c = (char)c;

  if (n % 4) {
    /* Must write one byte at a time */
    unsigned char *write_ptr = *ptr; /* careful - flex block must stay put */
    size_t i = 0;
    while (i < n) { /* unrolled loop (8 writes per iteration) */
      write_ptr[i] = c;
      if (++i >= n)
        break;

      write_ptr[i] = c;
      if (++i >= n)
        break;

      write_ptr[i] = c;
      if (++i >= n)
        break;

      write_ptr[i] = c;
      if (++i >= n)
        break;

      write_ptr[i] = c;
      if (++i >= n)
        break;

      write_ptr[i] = c;
      if (++i >= n)
        break;

      write_ptr[i] = c;
      if (++i >= n)
        break;

      write_ptr[i++] = c;
    }
  } else {
    /* Optimisation - can write words instead of bytes */
    uint32_t *write_ptr = *ptr; /* careful - flex block must stay put */
    const uint32_t wide = c |
                          (uint32_t)c << CHAR_BIT |
                          (uint32_t)c << (CHAR_BIT * 2) |
                          (uint32_t)c << (CHAR_BIT * 3);
    size_t i = 0;
    while (i < n/sizeof(*write_ptr)) {
      /* unrolled loop (8 writes per iteration) */
      write_ptr[i] = wide;
      if (++i >= n/sizeof(*write_ptr))
        break;

      write_ptr[i] = wide;
      if (++i >= n/sizeof(*write_ptr))
        break;

      write_ptr[i] = wide;
      if (++i >= n/sizeof(*write_ptr))
        break;

      write_ptr[i] = wide;
      if (++i >= n/sizeof(*write_ptr))
        break;

      write_ptr[i] = wide;
      if (++i >= n/sizeof(*write_ptr))
        break;

      write_ptr[i] = wide;
      if (++i >= n/sizeof(*write_ptr))
        break;

      write_ptr[i] = wide;
      if (++i >= n/sizeof(*write_ptr))
        break;

      write_ptr[i++] = wide;
    }
  }

  return ptr;
}

SFError check_trunc_or_ext(Reader *const reader, SFError err)
{
  if (!SFError_fail(err))
  {
    if (reader_ferror(reader))
    {
      err = SFERROR(ReadFail);
    }
    else if (reader_fgetc(reader) != EOF)
    {
      err = SFERROR(TooLong);
    }
    else if (reader_ferror(reader))
    {
      err = SFERROR(ReadFail);
    }
  }
  else if (reader_feof(reader))
  {
    err = SFERROR(Trunc);
  }

  return err;
}

int string_lcount(char const *const string, int *const max_width)
{
  assert(string);
  assert(max_width);

  int line_count = 0;
  char const *start = string;
  bool finished = false;
  do {
    _Optional char const *end = strchr(start, '\n');
    if (end == NULL) {
      end = start + strlen(start);
      finished = true;
    }
    ptrdiff_t nchars = end - start;
    assert(nchars >= 0);
    assert(nchars <= INT_MAX);
    *max_width = HIGHEST((int)nchars, *max_width);
    line_count++;
    start = end + 1;
  } while (!finished);

  return line_count;
}

