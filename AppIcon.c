/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Iconbar icon
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

/* ANSI library headers */
#include "stdlib.h"
#include <stdbool.h>
#include <assert.h>

/* RISC OS library files */
#include "event.h"
#include "toolbox.h"
#include "wimp.h"
#include "wimplib.h"
#include "iconbar.h"

#include "err.h"
#include "msgtrans.h"
#include "Macros.h"
#include "Loader3.h"
#include "debug.h"
#include "FileUtils.h"

#include "AppIcon.h"
#include "Utils.h"
#include "Session.h"
#include "filepaths.h"

enum
{
  WindowHandle_IconBar = -2, /* Pseudo window handle (icon bar) */
};

static ObjectId AppIcon_id = NULL_ObjectId;

/* ---------------- Private functions ---------------- */

static bool read_file(Reader *const reader, int const estimated_size,
  int const file_type, char const *const filename, void *const client_handle)
{
  NOT_USED(estimated_size);
  NOT_USED(client_handle);

  DataType const data_type = file_type_to_data_type(file_type, filename);

  if (data_type == DataType_Count) {
    report_error(SFERROR(BadFileType), filename, "");
    return false;
  }

  return Session_load_single(filename, data_type, reader);
}

static int datasave_message(WimpMessage *const message, void *const handle)
{
  /* Request to send us data */
  NOT_USED(handle);
  DEBUG("Icon bar received a DataSave message (ref. %d in reply to %d)",
        message->hdr.my_ref, message->hdr.your_ref);

  if (message->hdr.your_ref)
  {
    DEBUG("Icon bar ignoring a reply");
    return 0; /* message is a reply (will be dealt with by Entity module) */
  }

  DEBUG("Window handle is %d", message->data.data_save.destination_window);
  if (message->data.data_save.destination_window != WindowHandle_IconBar)
  {
    return 0;
  }

  DataType const data_type = file_type_to_data_type(
    message->data.data_save.file_type,
    message->data.data_save.leaf_name);

  if (data_type != DataType_Count) {
    E(loader3_receive_data(message, read_file, load_fail, NULL));
  } else {
    report_error(SFERROR(BadFileType), message->data.data_save.leaf_name, "");
  }

  return 1; /* claim message */
}

static int dataload_message(WimpMessage *const message, void *const handle)
{
  /* Request that we load data from a file */
  NOT_USED(handle);
  DEBUG("Icon bar received a DataLoad message (ref. %d in reply to %d)",
        message->hdr.my_ref, message->hdr.your_ref);

  if (message->hdr.your_ref)
  {
    DEBUG("Icon bar ignoring a reply");
    return 0; /* will be dealt with by Loader3 module */
  }

  DEBUG("Window handle is %d", message->data.data_load.destination_window);
  if (message->data.data_load.destination_window != WindowHandle_IconBar)
  {
    return 0;
  }

  char *filename = NULL;
  if (!E(canonicalise(&filename, NULL, NULL, message->data.data_load.leaf_name)))
  {
    DataType const data_type = file_type_to_data_type(
      message->data.data_load.file_type, filename);

    if (data_type != DataType_Count) {
      Session_open_single_file(filename, data_type);
    } else {
      report_error(SFERROR(BadFileType), filename, "");
    }
    free(filename);
  }

  /* Acknowledge that the file was loaded successfully
     (just a courtesy message, we don't expect a reply) */
  message->hdr.your_ref = message->hdr.my_ref;
  message->hdr.action_code = Wimp_MDataLoadAck;

  if (!E(wimp_send_message(Wimp_EUserMessage,
              message, message->hdr.sender, 0, NULL)))
  {
    DEBUGF("Sent DataLoadAck message (ref. %d)\n", message->hdr.my_ref);
  }

  return 1; /* claim message */
}

/* ---------------- Public functions ---------------- */

void AppIcon_created(ObjectId const id)
{
  AppIcon_id = id;

  /* Register Wimp message handlers to load files dropped on iconbar icon */
  EF(event_register_message_handler(Wimp_MDataSave, datasave_message, NULL));
  EF(event_register_message_handler(Wimp_MDataLoad, dataload_message, NULL));
}
