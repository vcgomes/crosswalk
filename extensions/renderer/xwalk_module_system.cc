// Copyright (c) 2013 Intel Corporation. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "xwalk/extensions/renderer/xwalk_module_system.h"

#include "base/logging.h"
#include "base/stl_util.h"
#include "xwalk/extensions/renderer/xwalk_extension_module.h"

namespace xwalk {
namespace extensions {

namespace {

// Index used to set embedder data into v8::Context, so we can get from a
// context to its corresponding module. Index chosen to not conflict with
// WebCore::V8ContextEmbedderDataField in V8PerContextData.h.
const int kModuleSystemEmbedderDataIndex = 8;

// This is the key used in the data object passed to our callbacks to store a
// pointer back to XWalkExtensionModule.
const char* kXWalkModuleSystem = "kXWalkModuleSystem";

XWalkModuleSystem* GetModuleSystem(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::HandleScope handle_scope(info.GetIsolate());
  v8::Handle<v8::Object> data = info.Data().As<v8::Object>();
  v8::Handle<v8::Value> module_system =
      data->Get(v8::String::New(kXWalkModuleSystem));
  if (module_system.IsEmpty() || module_system->IsUndefined()) {
    LOG(WARNING) << "Trying to use requireNative from already "
                 << "destroyed module system!";
    return NULL;
  }
  CHECK(module_system->IsExternal());
  return static_cast<XWalkModuleSystem*>(
      module_system.As<v8::External>()->Value());
}

void RequireNativeCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::ReturnValue<v8::Value> result(info.GetReturnValue());
  XWalkModuleSystem* module_system = GetModuleSystem(info);
  if (info.Length() < 1) {
    // TODO(cmarcelo): Throw appropriate exception or warning.
    result.SetUndefined();
    return;
  }
  v8::Handle<v8::Object> object =
      module_system->RequireNative(*v8::String::Utf8Value(info[0]));
  if (object.IsEmpty()) {
    // TODO(cmarcelo): Throw appropriate exception or warning.
    result.SetUndefined();
    return;
  }
  result.Set(object);
}

}  // namespace

XWalkModuleSystem::XWalkModuleSystem(v8::Handle<v8::Context> context) {
  v8::Isolate* isolate = context->GetIsolate();
  v8_context_.Reset(isolate, context);

  v8::HandleScope handle_scope(isolate);
  v8::Handle<v8::Object> function_data = v8::Object::New();
  function_data->Set(v8::String::New(kXWalkModuleSystem),
                     v8::External::New(this));
  v8::Handle<v8::FunctionTemplate> require_native_template =
      v8::FunctionTemplate::New(RequireNativeCallback, function_data);

  function_data_.Reset(isolate, function_data);
  require_native_template_.Reset(isolate, require_native_template);
}

XWalkModuleSystem::~XWalkModuleSystem() {
  DeleteExtensionModules();
  STLDeleteValues(&native_modules_);

  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::HandleScope handle_scope(isolate);

  // Deleting the data will disable the functions, they'll return early. We do
  // this because it might be the case that the JS objects we created outlive
  // this object, even if we destroy the references we have.
  // TODO(cmarcelo): Add a test for this case.
  // FIXME(cmarcelo): These calls are causing crashes on shutdown with Chromium
  //                  29.0.1547.57 and had to be commented out.
  // v8::Handle<v8::Object> function_data =
  //     v8::Handle<v8::Object>::New(isolate, function_data_);
  // function_data->Delete(v8::String::New(kXWalkModuleSystem));

  require_native_template_.Dispose();
  require_native_template_.Clear();
  function_data_.Dispose();
  function_data_.Clear();
  v8_context_.Dispose();
  v8_context_.Clear();
}

// static
XWalkModuleSystem* XWalkModuleSystem::GetModuleSystemFromContext(
    v8::Handle<v8::Context> context) {
  return reinterpret_cast<XWalkModuleSystem*>(
      context->GetAlignedPointerFromEmbedderData(
          kModuleSystemEmbedderDataIndex));
}

// static
void XWalkModuleSystem::SetModuleSystemInContext(
    scoped_ptr<XWalkModuleSystem> module_system,
    v8::Handle<v8::Context> context) {
  context->SetAlignedPointerInEmbedderData(kModuleSystemEmbedderDataIndex,
                                           module_system.release());
}

// static
void XWalkModuleSystem::ResetModuleSystemFromContext(
    v8::Handle<v8::Context> context) {
  delete GetModuleSystemFromContext(context);
  SetModuleSystemInContext(scoped_ptr<XWalkModuleSystem>(), context);
}

void XWalkModuleSystem::RegisterExtensionModule(
    scoped_ptr<XWalkExtensionModule> module,
    base::ListValue* entry_points) {
  const std::string& extension_name = module->extension_name();
  if (ContainsExtensionModule(extension_name)) {
    LOG(WARNING) << "Can't register Extension Module named for extension '"
                 << extension_name << "' in the Module System because name was "
                 << "already registered.";
    return;
  }
  extension_modules_.push_back(
      ExtensionModuleEntry(extension_name, module.release(), entry_points));
}

void XWalkModuleSystem::RegisterNativeModule(
    const std::string& name, scoped_ptr<XWalkNativeModule> module) {
  CHECK(native_modules_.find(name) == native_modules_.end());
  native_modules_[name] = module.release();
}

v8::Handle<v8::Object> XWalkModuleSystem::RequireNative(
    const std::string& name) {
  NativeModuleMap::iterator it = native_modules_.find(name);
  if (it == native_modules_.end())
    return v8::Handle<v8::Object>();
  return it->second->NewInstance();
}

void XWalkModuleSystem::Initialize() {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::HandleScope handle_scope(isolate);

  v8::Handle<v8::Context> context = GetV8Context();
  v8::Handle<v8::FunctionTemplate> require_native_template =
      v8::Handle<v8::FunctionTemplate>::New(isolate, require_native_template_);
  v8::Handle<v8::Function> require_native =
      require_native_template->GetFunction();

  MarkModulesWithTrampoline();

  ExtensionModules::iterator it = extension_modules_.begin();
  for (; it != extension_modules_.end(); ++it)
    it->module->LoadExtensionCode(context, require_native);
}

v8::Handle<v8::Context> XWalkModuleSystem::GetV8Context() {
  return v8::Handle<v8::Context>::New(v8::Isolate::GetCurrent(), v8_context_);
}

bool XWalkModuleSystem::ContainsExtensionModule(const std::string& name) {
  ExtensionModules::iterator it = extension_modules_.begin();
  for (; it != extension_modules_.end(); ++it) {
    if (it->name == name)
      return true;
  }
  return false;
}

void XWalkModuleSystem::DeleteExtensionModules() {
  for (ExtensionModules::iterator it = extension_modules_.begin();
       it != extension_modules_.end(); ++it) {
    delete it->module;
  }
  extension_modules_.clear();
}

// Returns whether the name of first is prefix of the second, considering "."
// character as a separator. So "a" is prefix of "a.b" but not of "ab".
bool XWalkModuleSystem::ExtensionModuleEntry::IsPrefix(
    const ExtensionModuleEntry& first,
    const ExtensionModuleEntry& second) {
  const std::string& p = first.name;
  const std::string& s = second.name;
  return s.size() > p.size() && s[p.size()] == '.'
      && std::mismatch(p.begin(), p.end(), s.begin()).first == p.end();
}

// Mark the extension modules that we want to setup "trampolines"
// instead of loading the code directly. The current algorithm is very
// simple: we only create trampolines for extensions that are leaves
// in the namespace tree.
//
// For example, if there are two extensions "tizen" and "tizen.time",
// the first one won't be marked with trampoline, but the second one
// will. So we'll only load code for "tizen" extension.
void XWalkModuleSystem::MarkModulesWithTrampoline() {
  std::sort(extension_modules_.begin(), extension_modules_.end());

  ExtensionModules::iterator it = extension_modules_.begin();
  while (it != extension_modules_.end()) {
    it = std::adjacent_find(it, extension_modules_.end(),
                            &ExtensionModuleEntry::IsPrefix);
    if (it == extension_modules_.end())
      break;
    VLOG(0) << it->name << " should not use trampoline.";
    it->use_trampoline = false;
    ++it;
  }
}

}  // namespace extensions
}  // namespace xwalk
