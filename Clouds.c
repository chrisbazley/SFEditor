/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Cloud colours
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
#include "Clouds.h"
#include "CloudsData.h"
#include "SFInit.h"

SFError clouds_read(CloudColData *const clouds,
  Reader *const reader)
{
  assert(clouds);

  for (size_t j = 0; j < Clouds_NumColours; ++j)
  {
    int const colour = reader_fgetc(reader);
    if (colour == EOF)
    {
      return SFERROR(ReadFail);
    }
    DEBUGF("Cloud colour[%zu]: %d\n", j, colour);
    assert(colour < NumColours);
    clouds->colours[j] = colour;
  }

  return SFERROR(OK);
}

void clouds_write(CloudColData const *const clouds,
  Writer *const writer)
{
  assert(clouds);

  for (size_t j = 0; j < Clouds_NumColours; ++j)
  {
    if (writer_fputc(clouds->colours[j], writer) == EOF)
    {
      break;
    }
  }
}


unsigned int clouds_get_colour(CloudColData const *const clouds, size_t const index)
{
  assert(clouds);
  assert(index < ARRAY_SIZE(clouds->colours));
  return clouds->colours[index];
}

void clouds_set_colour(CloudColData *const clouds,
  size_t const index, unsigned int const colour)
{
  assert(clouds);
  assert(index < Clouds_NumColours);
  assert(colour < NumColours);
  clouds->colours[index] = colour;
}
