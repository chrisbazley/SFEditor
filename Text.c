/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Text strings
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

#include <limits.h>
#include <inttypes.h>
#include <string.h>

#include "Debug.h"
#include "Macros.h"
#include "Reader.h"
#include "Writer.h"
#include "StringBuff.h"

#include "SFInit.h"
#include "SFError.h"
#include "Text.h"
#include "TextData.h"

enum {
  BytesPerAddresses = 8, // Overwritten on loading
  BytesPerStringNumber = 4, // Overwritten on loading
  BytesPerTextHeader = 48,
  DigitsStart = 0,
  AlphabetStart = 10,
  DefaultSpeed = 3,
  DefaultColour = 255, // White
  DefaultDuration = 5000,
  DefaultDelay = 0,
  DefaultCursorType = CursorType_None,
};

static int encode_char(int const ch)
{
  if ((ch >= 'A') && (ch <= 'Z'))
  {
    return (ch - 'A') + AlphabetStart;
  }

  if ((ch >= 'a') && (ch <= 'z'))
  {
    return (ch - 'a') + AlphabetStart;
  }

  if ((ch >= '0') && (ch <= '9'))
  {
    return (ch - '0') + DigitsStart;
  }

  switch (ch)
  {
    case '.': return 38;
    case '£': return 39;
    case '-': return 40;
    case '/': return 41;
    case '>': return 43;
    case '<': return 44;
    case '%': return 47;
    case '\'': return 48;
    case ' ': return 49;
    case '?': return 55;
    case '[': return 57;
    case ']': return 58;
    case '(': return 59;
    case ')': return 60;
    case ',': return 61;
    case ':': return 64;
    case '!': return 65;
    case '\n': return 254;
    case '\0': return 255;
  }
  return 49;
}

static int decode_char(int const ch)
{
  if (ch == EOF)
  {
    return ch;
  }

  if ((ch >= AlphabetStart) && ((ch - AlphabetStart) <= ('Z' - 'A')))
  {
    return (ch - AlphabetStart) + 'A';
  }

  if ((ch >= DigitsStart) && ((ch - DigitsStart) <= ('9' - '0')))
  {
    return (ch - DigitsStart) + '0';
  }

  switch (ch)
  {
    case 38:return '.';
    case 39:return '£';
    case 40:return '-';
    case 41:return '/';
    case 43:return '>';
    case 44:return '<';
    case 47:return '%';
    case 49:return ' ';
    case 48:return '\'';
    case 55:return '?';
    case 57:return '[';
    case 58:return ']';
    case 59:return '(';
    case 60:return ')';
    case 61:return ',';
    case 64:return ':';
    case 65:return '!';
    case 254:return '\n';
    case 255:return '\0'; /* string terminator */
  }
  return '#';
}

void text_destroy(Text *const text)
{
  assert(text != NULL);
  stringbuffer_destroy(&text->string);
}

void text_init(Text *const text)
{
  assert(text != NULL);
  *text = (Text){
    .params = {
      .duration = DefaultDuration,
      .delay = DefaultDelay,
      .speed = DefaultSpeed,
      .x_pos = 0,
      .y_pos = 0,
      .y_clip = Text_NoYClip,
      .repeat = false,
      .colour = DefaultColour,
      .cursor_type = DefaultCursorType,
    }
  };
  stringbuffer_init(&text->string);
}

char const *text_get_string(Text const *const text)
{
  assert(text != NULL);
  char const *const s = stringbuffer_get_pointer(&text->string);
  DEBUGF("Text string: '%s'\n", s);
  return s;
}

SFError text_set_string(Text *const text, char const *const string)
{
  assert(text != NULL);
  DEBUGF("Set text string '%s'\n", string);
  stringbuffer_truncate(&text->string, 0);

  size_t const len = strlen(string);
  size_t min_size = len + 1;
  char *buf = stringbuffer_prepare_append(&text->string, &min_size);
  if (!buf) {
    return SFERROR(NoMem);
  }

  // Round trip to ensure stored string is representative
  for (size_t i = 0; string[i] != '\0'; ++i) {
    assert(i < len);
    buf[i] = decode_char(encode_char(string[i]));
  }

  stringbuffer_finish_append(&text->string, len);
  return SFERROR(OK);
}

void text_set_params(Text *const text, TextParams const *const params)
{
  assert(text != NULL);
  assert(params != NULL);
  assert(params->duration > 0);
  assert(params->delay >= 0);
  assert(params->speed > 0);
  assert(params->x_pos >= 0);
  assert(params->x_pos < Text_NumColumns);
  assert(params->y_pos >= 0);
  assert(params->y_pos < Text_ScreenHeight);
  assert(params->cursor_type >= CursorType_None);
  assert(params->cursor_type < CursorType_Count);
  assert(params->colour >= 0);
  assert(params->colour < NumColours);

  text->params = *params;
}

void text_get_params(Text const *const text, TextParams *const params)
{
  assert(text != NULL);
  assert(params != NULL);
  *params = text->params;
}

void text_write_offset(Text const *const text, Writer *const writer,
  int *const offset)
{
  assert(text != NULL);
  assert(offset != NULL);
  writer_fwrite_int32(*offset, writer);
  *offset += BytesPerTextHeader + WORD_ALIGN((int)stringbuffer_get_length(&text->string) + 1);
}

void text_write_block(Text const *const text, Writer *const writer)
{
  assert(text != NULL);
  size_t const str_size = stringbuffer_get_length(&text->string) + 1; /* for terminator */
  char const *const str = stringbuffer_get_pointer(&text->string);

  writer_fseek(writer, BytesPerAddresses, SEEK_CUR);
  writer_fwrite_int32(text->params.duration, writer);
  writer_fwrite_int32(text->params.delay, writer);
  writer_fwrite_int32(text->params.speed, writer);
  writer_fwrite_int32(text->params.x_pos, writer);
  writer_fwrite_int32(text->params.y_pos, writer);
  writer_fseek(writer, BytesPerStringNumber, SEEK_CUR);
  writer_fwrite_int32(text->params.y_clip, writer);
  writer_fwrite_int32(text->params.repeat, writer);
  writer_fwrite_int32(text->params.cursor_type, writer);
  writer_fwrite_int32(text->params.colour, writer);

  for (size_t i = 0; i < str_size; ++i)
  {
    writer_fputc(encode_char(str[i]), writer);
  }
  assert(str_size <= LONG_MAX);
  writer_fseek(writer, WORD_ALIGN((long)str_size) - (long)str_size, SEEK_CUR);

  DEBUGF("Finished writing %zu-byte string, '%s', at %ld\n",
         str_size, str, writer_ftell(writer));
}

SFError text_read_block(Text *const text, Reader *const reader)
{
  assert(text != NULL);

  if (reader_fseek(reader, BytesPerAddresses, SEEK_CUR))
  {
    return SFERROR(BadSeek);
  }

  int32_t duration, delay, speed, x_pos, y_pos;
  if (!reader_fread_int32(&duration, reader) ||
      !reader_fread_int32(&delay, reader) ||
      !reader_fread_int32(&speed, reader) ||
      !reader_fread_int32(&x_pos, reader) ||
      !reader_fread_int32(&y_pos, reader))
  {
    return SFERROR(ReadFail);
  }

  DEBUGF("Duration: %" PRId32
         " Delay: %" PRId32
         " Speed: %" PRId32
         " X: %" PRId32
         " Y: %" PRId32 "\n",
         duration, delay, speed, x_pos, y_pos);

  if (duration < 0) {
    return SFERROR(BadTextDuration);
  }

  if (duration == 0) {
    duration = INT32_MAX;
  }

  if (delay < 0) {
    return SFERROR(BadTextDelay);
  }

  if (speed < 0) {
    return SFERROR(BadTextSpeed);
  }

  if (x_pos < 0 || x_pos >= Text_NumColumns) {
    return SFERROR(BadTextXPos);
  }

  if (y_pos < 0 || y_pos >= Text_ScreenHeight) {
    return SFERROR(BadTextYPos);
  }

  if (reader_fseek(reader, BytesPerStringNumber, SEEK_CUR))
  {
    return SFERROR(BadSeek);
  }

  int32_t y_clip, repeat, colour, cursor_type;
  if (!reader_fread_int32(&y_clip, reader) ||
      !reader_fread_int32(&repeat, reader) ||
      !reader_fread_int32(&cursor_type, reader) ||
      !reader_fread_int32(&colour, reader))
  {
    return SFERROR(ReadFail);
  }

  DEBUGF("Y clip: %" PRId32
         " Repeat: %" PRId32
         " Cursor: %" PRId32
         " Colour: %" PRId32 "\n",
         y_clip, repeat, cursor_type, colour);

  if (y_clip < 0 || (y_clip > Text_NumRows && y_clip != Text_NoYClip)) {
    return SFERROR(BadTextYClip);
  }

  if (repeat != 0 && repeat != 1) {
    return SFERROR(BadTextRepeat);
  }

  if (cursor_type < CursorType_None && cursor_type >= CursorType_Count) {
    return SFERROR(BadTextCursorType);
  }

  if (colour < 0 && colour >= NumColours) {
    return SFERROR(BadTextColour);
  }

  *text = (Text){
    .params = {
      .duration = duration,
      .delay = delay,
      .speed = speed,
      .x_pos = x_pos,
      .y_pos = y_pos,
      .y_clip = y_clip,
      .repeat = repeat,
      .colour = colour,
      .cursor_type = cursor_type,
    }
  };

  stringbuffer_init(&text->string);

  for (int ch = decode_char(reader_fgetc(reader));
       ch != '\0';
       ch = decode_char(reader_fgetc(reader)))
  {
    if (ch == EOF)
    {
      return SFERROR(ReadFail);
    }

    char const tmp[2] = {ch, '\0'};
    if (!stringbuffer_append_all(&text->string, tmp))
    {
      return SFERROR(NoMem);
    }
  }

  DEBUGF("Finished reading %zu-character string, '%s', at %ld\n",
         stringbuffer_get_length(&text->string),
         stringbuffer_get_pointer(&text->string), reader_ftell(reader));

  return SFERROR(OK);
}

