/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Main program skeleton
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

/* ANSI library files */
#include "stdlib.h"

/* RISC OS library files */
#include "event.h"
#include "wimp.h"

#include "err.h"
#include "Debug.h"
#include "scheduler.h"

#include "ParseArgs.h"
#include "SFInit.h"

/* ---------------- Private functions ---------------- */

#ifdef FORTIFY
static bool fortify_detected = false, fortify_in_scope = false;

static void fortify_check(void)
{
  Fortify_CheckAllMemory();
  if (fortify_in_scope)
  {
    Fortify_LeaveScope();
  }
  assert(!fortify_detected);
}

/* ----------------------------------------------------------------------- */

static void fortify_output(char const *text)
{
  DEBUGF(text);
  if (strstr(text, "Fortify"))
  {
    assert(!fortify_detected);
  }
  if (strstr(text, "detected"))
  {
    fortify_detected = true;
  }
}
#endif

/* ---------------- Public functions ---------------- */

int main(int argc, char *argv[])
{
  DEBUG_SET_OUTPUT(DebugOutput_Reporter, APP_NAME);

#ifdef FORTIFY
  Fortify_SetOutputFunc(fortify_output);
  atexit(fortify_check);
#endif

  initialise();

#ifdef FORTIFY
  { /* Wait for idle time (after object auto-creation event delivery) */
    unsigned int mask;
    int event_code;
    EF(event_get_mask(&mask));

    EF(event_set_mask(0));
    do
    {
      EF(event_poll(&event_code, NULL, NULL));
    }
    while (event_code != Wimp_ENull);
    EF(event_set_mask(mask));
  }

  /* We deliberately don't count any memory allocated during initialization
     as leaked, e.g. persistent event handlers that are never deregistered. */
  Fortify_EnterScope();
  fortify_in_scope = true;
#endif

  parse_arguments(argc, argv);

  /*
   * poll loop
   */

  while (1)
  {
#ifdef FORTIFY
    Fortify_CheckAllMemory();
#endif
    E(scheduler_poll(NULL, NULL, NULL));
  }

  return EXIT_SUCCESS; /* unreachable */
}
