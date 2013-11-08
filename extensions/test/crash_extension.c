// Copyright (c) 2013 Intel Corporation. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if defined(__cplusplus)
#error "This file is written in C to make sure the C API works as intended."
#endif

#include <stdio.h>
#include <stdlib.h>
#include "xwalk/extensions/public/XW_Extension.h"
#include "xwalk/extensions/public/XW_Extension_SyncMessage.h"

XW_Extension g_extension = 0;
const XW_CoreInterface* g_core = NULL;
const XW_MessagingInterface* g_messaging = NULL;

void handle_message(XW_Instance instance, const char* message) {
  printf("From JavaScript [%s], going to crash now!\n", message);

  int* boom = NULL;
  *boom = 0;

  g_messaging->PostMessage(instance, "Didn't crash!");
}

int32_t XW_Initialize(XW_Extension extension, XW_GetInterface get_interface) {
  static const char* kAPI =
      "var afterCrash = null;"
      "extension.setMessageListener(function(msg) {"
      "  if (afterCrash instanceof Function) {"
      "    afterCrash(msg);"
      "  };"
      "});"
      "exports.crash = function(msg, callback) {"
      "  afterCrash = callback;"
      "  extension.postMessage(msg);"
      "};";

  g_extension = extension;
  g_core = get_interface(XW_CORE_INTERFACE);
  g_core->SetExtensionName(extension, "crash");
  g_core->SetJavaScriptAPI(extension, kAPI);

  g_messaging = get_interface(XW_MESSAGING_INTERFACE);
  g_messaging->Register(extension, handle_message);

  return XW_OK;
}
