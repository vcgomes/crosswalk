// Copyright (c) 2013 Intel Corporation. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef XWALK_APPLICATION_BROWSER_APPLICATION_SYSTEM_H_
#define XWALK_APPLICATION_BROWSER_APPLICATION_SYSTEM_H_

#include <map>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/exported_object.h"
#include "base/logging.h"
#include "xwalk/application/browser/application_process_manager.h"
#include "xwalk/application/browser/application_service.h"

namespace xwalk {
class RuntimeContext;
}

namespace xwalk {
namespace application {

class ApplicationLister {
 public:
  ApplicationLister(ApplicationService* service);
  ~ApplicationLister();

  void List(dbus::MethodCall* method_call,
            dbus::ExportedObject::ResponseSender response_sender);

 private:
  void OnExported(const std::string& interface_name,
                  const std::string& method_name,
                  bool success) {
    LOG(WARNING) << "Exported in " << interface_name << ": " << success;
  }

  ApplicationService* application_service_;
  dbus::ExportedObject* lister_;
  scoped_refptr<dbus::Bus> session_;
};

// The ApplicationSystem manages the creation and destruction of services which
// related to applications' runtime model.
// There's one-to-one correspondence between ApplicationSystem and
// RuntimeContext.
class ApplicationSystem {
 public:
  explicit ApplicationSystem(xwalk::RuntimeContext* runtime_context);
  ~ApplicationSystem();

  // The ApplicationProcessManager is created at startup.
  ApplicationProcessManager* process_manager() {
    return process_manager_.get();
  }

  // The ApplicationService is created at startup.
  ApplicationService* application_service() {
    return application_service_.get();
  }

 private:
  xwalk::RuntimeContext* runtime_context_;
  scoped_ptr<ApplicationProcessManager> process_manager_;
  scoped_ptr<ApplicationService> application_service_;
  scoped_ptr<ApplicationLister> application_lister_;

  DISALLOW_COPY_AND_ASSIGN(ApplicationSystem);
};

}  // namespace application
}  // namespace xwalk

#endif  // XWALK_APPLICATION_BROWSER_APPLICATION_SYSTEM_H_
