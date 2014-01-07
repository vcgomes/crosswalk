// Copyright (c) 2013 Intel Corporation. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "xwalk/application/browser/installer/tizen/package_installer.h"

#include <sys/types.h>
#include <pwd.h>
#include <unistd.h>
#include <pkgmgr/pkgmgr_parser.h>

#include <algorithm>
#include <string>
#include "base/file_util.h"
#include "base/files/file_enumerator.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/command_line.h"
#include "base/process/launch.h"
#include "third_party/libxml/chromium/libxml_utils.h"
#include "xwalk/application/browser/application_storage.h"
#include "xwalk/application/browser/installer/tizen/packageinfo_constants.h"

namespace {

const base::FilePath kPkgHelper("/usr/bin/xwalk-pkg-helper");

}

namespace info = xwalk::application_packageinfo_constants;

namespace xwalk {
namespace application {

PackageInstaller::~PackageInstaller() {
}

// static
scoped_ptr<PackageInstaller> PackageInstaller::Create(
    ApplicationService* service,
    ApplicationStorage* storage,
    const std::string& package_id,
    const base::FilePath& data_dir) {
  if (!base::PathExists(data_dir))
    return scoped_ptr<PackageInstaller>();
  scoped_ptr<PackageInstaller> handler(
      new PackageInstaller(service, storage, package_id, data_dir));
  if (!handler->Init())
    return scoped_ptr<PackageInstaller>();
  return handler.Pass();
}

PackageInstaller::PackageInstaller(
    ApplicationService* service,
    ApplicationStorage* storage,
    const std::string& package_id,
    const base::FilePath& data_dir)
    : service_(service)
    , storage_(storage)
    , package_id_(package_id)
    , data_dir_(data_dir) {
  CHECK(service_);
}

bool PackageInstaller::Init() {
  app_dir_ = data_dir_.Append(info::kAppDir).AppendASCII(package_id_);
  xml_path_ = base::FilePath(app_dir_).AppendASCII(
      package_id_ + std::string(info::kXmlExtension));
  execute_path_ = app_dir_.Append(info::kExecDir).AppendASCII(package_id_);

  application_ = storage_->GetApplicationData(package_id_);
  if (!application_) {
    LOG(ERROR) << "Application " << package_id_
               << " haven't been installed in Xwalk database.";
    return false;
  }
  stripped_name_ = application_->Name();
  stripped_name_.erase(
      std::remove_if(stripped_name_.begin(), stripped_name_.end(), ::isspace),
      stripped_name_.end());

  if (!application_->GetManifest()->GetString(info::kIconKey, &icon_name_))
    LOG(WARNING) << "Fail to get application icon name.";

  // FIXME(vcgomes): Add support for more icon types.
  icon_path_ = base::FilePath(info::kIconDir).AppendASCII(package_id_ + ".png");
  return true;
}

bool PackageInstaller::GeneratePkgInfoXml() {
  base::FilePath dir_xml(xml_path_.DirName());
  if (!base::PathExists(dir_xml) &&
      !file_util::CreateDirectory(dir_xml))
    return false;

  FILE* file = file_util::OpenFile(xml_path_, "w");
  XmlWriter xml_writer;
  xml_writer.StartWriting();
  xml_writer.StartElement("manifest");
  xml_writer.AddAttribute("xmlns", "http://tizen.org/ns/packages");
  xml_writer.AddAttribute("package", package_id_);
  xml_writer.AddAttribute("version", application_->VersionString());
  xml_writer.WriteElement("label", application_->Name());
  xml_writer.WriteElement("description", application_->Description());

  {
    xml_writer.StartElement("ui-application");
    xml_writer.AddAttribute("appid",
        package_id_ + info::kSeparator + stripped_name_);
    xml_writer.AddAttribute("exec", execute_path_.MaybeAsASCII());
    xml_writer.AddAttribute("type", "c++app");
    xml_writer.AddAttribute("taskmanage", "true");
    xml_writer.WriteElement("label", application_->Name());
    if (icon_name_.empty())
      xml_writer.WriteElement("icon", info::kDefaultIconName);
    else
      xml_writer.WriteElement("icon", icon_path_.BaseName().MaybeAsASCII());
    xml_writer.EndElement();  // Ends "ui-application"
  }

  xml_writer.EndElement();  // Ends "manifest" element.
  xml_writer.StopWriting();

  file_util::WriteFile(xml_path_,
                       xml_writer.GetWrittenString().c_str(),
                       xml_writer.GetWrittenString().size());
  file_util::CloseFile(file);
  LOG(INFO) << "Converting manifest.json into "
            << xml_path_.BaseName().MaybeAsASCII()
            << " for installation. [DONE]";
  return true;
}

bool PackageInstaller::Install() {
  if (!GeneratePkgInfoXml())
    return false;

  base::FilePath icon = app_dir_.AppendASCII("/src/").AppendASCII(icon_name_);

  LOG(WARNING) << "icon " << icon.value();
  LOG(WARNING) << "package id " << package_id_;
  LOG(WARNING) << "xml " << xml_path_.value();

  CommandLine cmdline(kPkgHelper);
  cmdline.AppendSwitch("--install");
  cmdline.AppendArg(package_id_);
  cmdline.AppendArgPath(xml_path_);
  cmdline.AppendArgPath(icon);

  int exit_code;
  std::string output;

  if (!base::GetAppOutputWithExitCode(cmdline, &output, &exit_code)) {
    LOG(ERROR) << "Could launch installer helper";
    return false;
  }

  if (exit_code != 0) {
    LOG(ERROR) << "Could not install application: "
               << output << " (" << exit_code << ")";
    return false;
  }

  return true;
}

bool PackageInstaller::Uninstall() {
  CommandLine cmdline(kPkgHelper);
  cmdline.AppendSwitch("--uninstall");
  cmdline.AppendArg(package_id_);

  int exit_code;
  std::string output;

  if (!base::GetAppOutputWithExitCode(cmdline, &output, &exit_code)) {
    LOG(ERROR) << "Could launch installer helper";
    return false;
  }

  if (exit_code != 0) {
    LOG(ERROR) << "Could not uninstall application: "
               << output << " (" << exit_code << ")";
    return false;
  }

  return true;
}

}  // namespace application
}  // namespace xwalk
