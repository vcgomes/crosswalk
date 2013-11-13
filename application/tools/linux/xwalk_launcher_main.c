// Copyright (c) 2013 Intel Corporation. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

#include <glib.h>
#include <gio/gio.h>

#if defined(OS_TIZEN_MOBILE)
#include <appcore/appcore-common.h>

// Private struct from appcore-internal, necessary to get events from the system
struct bundle;

enum app_event {
	AE_UNKNOWN,
	AE_CREATE,
	AE_TERMINATE,
	AE_PAUSE,
	AE_RESUME,
	AE_RESET,
	AE_LOWMEM_POST,
	AE_MEM_FLUSH,
	AE_MAX
};

struct ui_ops {
	void *data;
	void (*cb_app) (enum app_event evnt, void *data, bundle *b);
};
#endif  // defined(OS_TIZEN_MOBILE)

static const char* xwalk_service_name = "org.crosswalkproject";
static const char* xwalk_running_path = "/running";
static const char* xwalk_running_manager_iface =
    "org.crosswalkproject.Running.Manager";
static const char* xwalk_running_app_iface =
    "org.crosswalkproject.Running.Application";

static char* application_object_path;

static GMainLoop* mainloop;

static void object_removed(GDBusObjectManager* manager, GDBusObject* object,
                           gpointer user_data) {
  const char* path = g_dbus_object_get_object_path(object);

  if (g_strcmp0(path, application_object_path))
    return;

  fprintf(stderr, "Application '%s' disappeared, exiting.\n", path);

  g_main_loop_quit(mainloop);
}

static void on_app_properties_changed(GDBusProxy* proxy,
                                      GVariant* changed_properties,
                                      GStrv invalidated_properties,
                                      gpointer user_data) {
  const char* interface = g_dbus_proxy_get_interface_name(proxy);

  fprintf(stderr, "properties changed %s\n", interface);

  if (g_variant_n_children(changed_properties) == 0)
    return;

  if (g_strcmp0(interface, xwalk_running_app_iface))
    return;

  GVariantIter* iter;
  const gchar* key;
  GVariant* value;

  g_variant_get(changed_properties, "a{sv}", &iter);

  while (g_variant_iter_loop(iter, "{&sv}", &key, &value)) {
    if (g_strcmp0(key, "State"))
      continue;

    const gchar* state = g_variant_get_string(value, NULL);

    fprintf(stderr, "Application state %s\n", state);
  }
}

#if defined(OS_TIZEN_MOBILE)

static const char *event2str(enum app_event event) {
  switch(event) {
    case AE_UNKNOWN:
      return "AE_UNKNOWN";
    case AE_CREATE:
      return "AE_CREATE";
    case AE_TERMINATE:
      return "AE_TERMINATE";
    case AE_PAUSE:
      return "AE_PAUSE";
    case AE_RESUME:
      return "AE_RESUME";
    case AE_RESET:
      return "AE_RESET";
    case AE_LOWMEM_POST:
      return "AE_LOWMEM_POST";
    case AE_MEM_FLUSH:
      return "AE_MEM_FLUSH";
    case AE_MAX:
      return "AE_MAX";
  }

  return "INVALID EVENT";
}

static void application_event_cb(enum app_event event, void *data, bundle *b) {
  fprintf(stderr, "event %s\n", event2str(event));
}
#endif  // defined(OS_TIZEN_MOBILE)

int main(int argc, char* argv[]) {
  GError* error = NULL;
  const char* appid;

#if !GLIB_CHECK_VERSION(2, 36, 0)
  // g_type_init() is deprecated on GLib since 2.36, Tizen has 2.32.
  g_type_init();
#endif

  if (argc >= 2) {
    appid = argv[1];
  } else {
    // We assume that we are running from a link to xwalk binary.
    appid = argv[0];
  }

  GDBusObjectManager* running_apps_om = g_dbus_object_manager_client_new_for_bus_sync(
      G_BUS_TYPE_SESSION, G_DBUS_OBJECT_MANAGER_CLIENT_FLAGS_NONE,
      xwalk_service_name, xwalk_running_path,
      NULL, NULL, NULL, NULL, NULL);
  if (!running_apps_om) {
    fprintf(stderr, "Service '%s' does could not be reached\n", xwalk_service_name);
    exit(1);
  }

  g_signal_connect(running_apps_om, "object-removed",
                   G_CALLBACK(object_removed), NULL);

  GDBusProxy* running_proxy = g_dbus_proxy_new_for_bus_sync(G_BUS_TYPE_SESSION,
		  G_DBUS_PROXY_FLAGS_NONE, NULL, xwalk_service_name,
		  xwalk_running_path, xwalk_running_manager_iface, NULL, &error);
  if (!running_proxy) {
    g_print("Couldn't create proxy for '%s': %s\n", xwalk_running_manager_iface,
            error->message);
    g_error_free(error);
    exit(1);
  }

  GVariant* result = g_dbus_proxy_call_sync(running_proxy, "Launch",
                                            g_variant_new("(s)", appid),
                                            G_DBUS_CALL_FLAGS_NONE,
                                            -1, NULL, &error);
  if (!result) {
    fprintf(stderr, "Couldn't call 'Launch' method: %s\n", error->message);
    exit(1);
  }

  g_variant_get(result, "(o)", &application_object_path);
  fprintf(stderr, "Application launched with path '%s'\n", application_object_path);

  GDBusProxy* app_proxy = g_dbus_proxy_new_for_bus_sync(
      G_BUS_TYPE_SESSION,
      G_DBUS_PROXY_FLAGS_NONE, NULL, xwalk_service_name,
      application_object_path, xwalk_running_app_iface, NULL, &error);
  if (!app_proxy) {
    g_print("Couldn't create proxy for '%s': %s\n", xwalk_running_app_iface,
            error->message);
    g_error_free(error);
    exit(1);
  }

  g_signal_connect(app_proxy, "g-properties-changed",
                   G_CALLBACK(on_app_properties_changed), NULL);

  mainloop = g_main_loop_new(NULL, FALSE);

#if defined(OS_TIZEN_MOBILE)
  char name[128];
  snprintf(name, sizeof(name), "xwalk-%s", appid);

  struct ui_ops ops;
  memset(&ops, 0, sizeof(ops));
  ops.cb_app = application_event_cb;

  int ret = appcore_init(name, &ops, argc, argv);
  if (ret) {
    fprintf(stderr, "Failed to initialize appcore");
    exit(1);
  }
#endif  // defined(OS_TIZEN_MOBILE)

  g_main_loop_run(mainloop);

  return 0;
}
