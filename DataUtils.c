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
#include <ctype.h>
#include <stdio.h>
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


_Optional char *read_line_comm(char *const s, size_t const n, FILE *const stream,
                               _Optional int *const line_num)
{
  /* Read string from file into buffer s of length n,
     ignoring comments and blank lines.
     Returns s if successful; otherwise, NULL for a read error,
     for EOF, or for an overlong line. */
  assert(s);
  assert(stream);
  assert(n >= 2);
  assert(n <= INT_MAX);
  _Optional char *err;
  size_t len = 0;

  do {
    /* Read line */
    if (line_num != NULL)
      (*line_num)++;
    err = fgets(s, (int)n, stream);

    if (err != NULL) {
      len = strlen(s);
      if (len == n - 1 && s[len - 1] != '\n') {
        err = NULL; /* Line is too long */
      }
    }
  /* skip comments and blank lines */
  } while (err != NULL && (*s == '#' || *s == '\n'));

  /* strip trailing spaces and add newline in case necessary */
  if (err != NULL) {
    /* work backwards from end of string */
    size_t c;
    for (c = len; c > 0 && isspace((unsigned char)s[c-1]); c--) {
    }
    /* c is the index of the first character of any trailing white-space
       in the string, or the index of the string's terminator */
    assert(c+1 < n);
    s[c] = '\n';
    s[c+1] = '\0';
  }
  return err;
}
