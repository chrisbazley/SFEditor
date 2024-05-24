/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Mission briefing text
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

#include "stdlib.h"
#include <string.h>

#include "Debug.h"
#include "Macros.h"
#include "Reader.h"
#include "Writer.h"
#include "StringBuff.h"

#include "Utils.h"
#include "Text.h"
#include "TextData.h"
#include "Briefing.h"
#include "BriefingData.h"
#include "SFError.h"

typedef enum {
  BriefingTextIndex_Title,
  BriefingTextIndex_LocDate,
} BriefingTextIndex;

enum {
  TopMargin = 8,
  LeftMargin = 2,
  BriefingDuration = 5000,
  BriefingSpeed = 3,
  BriefingDelay = 0,
  TitleColour = 251,
  TitleDelay = 25,
  TitleSpeed = 1,
  BriefingColour1 = 255,
  BriefingColour2 = 119,
  MaxNumRows = 34,
  MaxNumCols = Text_NumColumns - 1,
};

void briefing_init(BriefingData *const briefing)
{
  assert(briefing);
  *briefing = (BriefingData){
    .count = 0,
    .texts = NULL,
    .line_count = TopMargin,
    .title_colour = TitleColour,
    .brief_colours = {BriefingColour1, BriefingColour2},
    .cindex = 0,
  };
}

void briefing_destroy(BriefingData *const briefing)
{
  assert(briefing);
  for (size_t i = 0; i < briefing->count; ++i) {
    text_destroy(&briefing->texts[i]);
  }
  free(briefing->texts);
}

int briefing_write_text_offsets(BriefingData *const briefing, Writer *const writer,
  int offset)
{
  assert(briefing);

  for (size_t i = 0; i < briefing->count; ++i)
  {
    text_write_offset(&briefing->texts[i], writer, &offset);
    if (writer_ferror(writer))
    {
      break;
    }
  }

  return offset;
}

void briefing_write_texts(BriefingData *const briefing, Writer *const writer)
{
  assert(briefing);

  for (size_t i = 0; i < briefing->count; ++i)
  {
    text_write_block(&briefing->texts[i], writer);
    if (writer_ferror(writer))
    {
      return;
    }
  }
}

SFError briefing_read_texts(BriefingData *const briefing,
                            long int const *const offsets, size_t const count,
                            Reader *const reader)
{
  assert(briefing);
  assert(offsets);
  assert(count >= 0);
  assert(count <= BriefingMax);

  briefing->texts = malloc(sizeof(briefing->texts[0]) * count);
  if (!briefing->texts)
  {
    return SFERROR(NoMem);
  }

  for (size_t i = 0; i < count; ++i)
  {
    text_init(&briefing->texts[i]);
  }

  briefing->count = count;

  for (size_t i = 0; i < count; ++i)
  {
    if (reader_fseek(reader, offsets[i], SEEK_SET))
    {
      return SFERROR(BadSeek);
    }

    SFError const err = text_read_block(&briefing->texts[i], reader);
    if (SFError_fail(err))
    {
      return err;
    }
  }

  return SFERROR(OK);
}

size_t briefing_get_text_count(BriefingData const *const briefing)
{
  assert(briefing);
  assert(briefing->count >= 0);
  assert(briefing->count <= BriefingMax);
  return briefing->count;
}

char const *briefing_get_text(BriefingData const *const briefing, size_t const index)
{
  assert(briefing);
  assert(briefing->texts);
  assert(briefing->count >= 0);
  assert(briefing->count <= BriefingMax);
  assert(index < briefing->count);
  return text_get_string(&briefing->texts[index]);
}

SFError briefing_add_text(BriefingData *const briefing,
  char const *const string)
{
  assert(briefing);
  assert(briefing->count >= 0);
  assert(briefing->count <= BriefingMax);
  assert(string);
  DEBUGF("Add string '%s'\n", string);

  if (briefing->count >= BriefingMax) {
    return SFERROR(TooManyBriefingStrings);
  }

  int max_width = 0;
  int const line_count = string_lcount(string, &max_width);

  if (LeftMargin + max_width > MaxNumCols) {
    return SFERROR(TooManyBriefingColumns);
  }

  if (briefing->line_count + line_count > MaxNumRows) {
    return SFERROR(TooManyBriefingLines);
  }

  Text *const texts = realloc(briefing->texts,
                        sizeof(briefing->texts[0]) * (briefing->count + 1));
  if (!texts) {
    return SFERROR(NoMem);
  }
  briefing->texts = texts;

  size_t const index = briefing->count;
  text_init(&briefing->texts[index]);

  SFError err = text_set_string(&briefing->texts[index], string);
  if (SFError_fail(err)) {
    return err;
  }

  if (index == BriefingTextIndex_Title) {
    TextParams params = {
      .duration = BriefingDuration,
      .delay = TitleDelay,
      .speed = TitleSpeed,
      .x_pos = LeftMargin,
      .y_pos = briefing->line_count * Text_CharHeight,
      .y_clip = Text_NoYClip,
      .repeat = false,
      .colour = briefing->title_colour,
      .cursor_type = CursorType_Block,
    };
    text_set_params(&briefing->texts[index], &params);
  } else {
    TextParams params = {
      .duration = BriefingDuration,
      .delay = BriefingDelay,
      .speed = BriefingSpeed,
      .x_pos = LeftMargin,
      .y_pos = briefing->line_count * Text_CharHeight,
      .y_clip = Text_NoYClip,
      .repeat = false,
      .colour = briefing->brief_colours[briefing->cindex],
      .cursor_type = CursorType_None,
    };
    text_set_params(&briefing->texts[index], &params);
  }

  briefing->line_count += line_count;
  DEBUGF("Line count is now %d (added %d)\n", briefing->line_count, line_count);

  if (index == BriefingTextIndex_Title) {
    // There's an implicit newline after the title
    briefing->line_count++;
  } else {
    assert(briefing->cindex <= 1);
    briefing->cindex = 1 - briefing->cindex;
  }

  ++briefing->count;
  return SFERROR(OK);
}

static char const *get_prefixed_text(char const *const string)
{
  char const *prefix_end = strchr(string, ':');
  if (!prefix_end) {
    DEBUGF("Prefix not found in '%s'\n", string);
    return "";
  }
  ++prefix_end;
  while (*prefix_end == ' ') {
    ++prefix_end;
  }
  return prefix_end;
}

static char const *get_text(BriefingData const *const briefing,
  BriefingTextIndex const index)
{
  assert(briefing);
  if (index >= briefing->count) {
    DEBUGF("Text string %d not found\n", (int)index);
    return "";
  }
  return get_prefixed_text(briefing_get_text(briefing, index));
}

char const *briefing_get_title(BriefingData const *const briefing)
{
  return get_text(briefing, BriefingTextIndex_Title);
}
