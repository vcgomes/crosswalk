// Copyright (c) 2013 Intel Corporation. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"

#include "xwalk/application/browser/application_system.h"
#include "content/public/browser/browser_thread.h"
#include "xwalk/application/browser/application_process_manager.h"
#include "xwalk/application/browser/application_service.h"
#include "xwalk/application/browser/application_store.h"
#include "xwalk/runtime/browser/runtime_context.h"
#include "dbus/object_path.h"

using xwalk::RuntimeContext;

namespace {

const std::string kServiceName("org.xwalk");
const std::string kInterfaceName("org.xwalk.ApplicationLister1");
const dbus::ObjectPath kListerObjectPath("/running");
const std::string kListMethodName("List");

}  // namespace

namespace xwalk {
namespace application {

ApplicationLister::ApplicationLister(ApplicationService* service)
    : application_service_(service) {
  dbus::Bus::Options options;
  options.dbus_task_runner = content::BrowserThread::GetMessageLoopProxyForThread(
      content::BrowserThread::IO);
  session_ = new dbus::Bus(options);

  session_->RequestOwnership(
      kServiceName, dbus::Bus::REQUIRE_PRIMARY,
      base::Bind(&ApplicationLister::OnNameOwned, base::Unretained(this)));
}

ApplicationLister::~ApplicationLister() {
  session_->ShutdownAndBlock();
}

void ApplicationLister::OnNameOwned(const std::string& name, bool success) {
  if (!success) {
    LOG(ERROR) << "Cound not own service name '" << name << "'.";
    return;
  }

  lister_ = session_->GetExportedObject(kListerObjectPath);

  lister_->ExportMethod(
      kInterfaceName, kListMethodName,
      base::Bind(&ApplicationLister::List, base::Unretained(this)),
      base::Bind(&ApplicationLister::OnExported, base::Unretained(this)));
}

void ApplicationLister::List(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  scoped_ptr<dbus::Response> response = dbus::Response::FromMethodCall(method_call);
  dbus::MessageWriter top_writer(response.get());

  xwalk::application::ApplicationStore::ApplicationMap* apps =
      application_service_->GetInstalledApplications();
  xwalk::application::ApplicationStore::ApplicationMapIterator it;

  dbus::MessageWriter app_writer(NULL);
  top_writer.OpenArray("(ss)", &app_writer);

  for (it = apps->begin(); it != apps->end(); ++it) {
    dbus::MessageWriter entry_writer(NULL);
    app_writer.OpenStruct(&entry_writer);

    entry_writer.AppendString(it->first);
    entry_writer.AppendString(it->second->Name());

    app_writer.CloseContainer(&entry_writer);
  }

  top_writer.CloseContainer(&app_writer);

  response_sender.Run(response.Pass());
}

ApplicationSystem::ApplicationSystem(RuntimeContext* runtime_context) {
  runtime_context_ = runtime_context;
  process_manager_.reset(new ApplicationProcessManager(runtime_context));
  application_service_.reset(new ApplicationService(runtime_context));
  application_lister_.reset(new ApplicationLister(application_service_.get()));
}

ApplicationSystem::~ApplicationSystem() {
}

}  // namespace application
}  // namespace xwalk
