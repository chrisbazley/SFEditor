/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Mission position in pyramid
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

#include "Debug.h"
#include "Macros.h"
#include "Reader.h"
#include "Writer.h"

#include "SFError.h"
#include "PyramData.h"
#include "Pyram.h"

enum {
  MinLevelNumber = 1,
  MaxLevelNumber = 36,
};

SFError pyramid_read(PyramidData *pyramid, Reader *reader)
{
  assert(pyramid);

  int32_t level_number = 0;
  if (!reader_fread_int32(&level_number, reader))
  {
    return SFERROR(ReadFail);
  }

  if (level_number < MinLevelNumber || level_number > MaxLevelNumber)
  {
    return SFERROR(BadLevelNumber);
  }

  int32_t difficulty = 0;
  if (!reader_fread_int32(&difficulty, reader))
  {
    return SFERROR(ReadFail);
  }

  if (difficulty < Pyramid_Easy || difficulty > Pyramid_User)
  {
    return SFERROR(BadPyramid);
  }

  if (difficulty == Pyramid_User && level_number != 1)
  {
    return SFERROR(BadLevelNumber);
  }

  *pyramid = (PyramidData){
    .difficulty = difficulty,
    .level_number = level_number,
  };
  DEBUGF("Finished reading pyramid data at %ld\n", reader_ftell(reader));
  return SFERROR(OK);
}

void pyramid_write(PyramidData const *pyramid, Writer *writer)
{
  assert(pyramid);

  assert(pyramid->level_number >= MinLevelNumber);
  assert(pyramid->level_number <= MaxLevelNumber);
  writer_fwrite_int32(pyramid->level_number, writer);

  assert(pyramid->difficulty >= Pyramid_Easy);
  assert(pyramid->difficulty <= Pyramid_User);
  writer_fwrite_int32(pyramid->difficulty, writer);
  DEBUGF("Finished writing pyramid data at %ld\n", writer_ftell(writer));
}


void pyramid_set_position(PyramidData *const pyramid,
  Pyramid const difficulty, int const level_number)
{
  assert(pyramid);

  assert(difficulty >= Pyramid_Easy);
  assert(difficulty <= Pyramid_User);
  pyramid->difficulty = difficulty;

  assert(level_number >= MinLevelNumber);
  assert(level_number <= MaxLevelNumber);
  if (difficulty == Pyramid_User)
  {
    assert(level_number == 1);
  }
  pyramid->level_number = level_number;
}

int pyramid_get_level_number(PyramidData const *const pyramid)
{
  assert(pyramid);
  return pyramid->level_number;
}

Pyramid pyramid_get_difficulty(PyramidData const *const pyramid)
{
  assert(pyramid);
  return pyramid->difficulty;
}
