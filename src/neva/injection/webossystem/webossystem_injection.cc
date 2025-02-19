// Copyright 2014-2019 LG Electronics, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// SPDX-License-Identifier: Apache-2.0


#include "neva/injection/webossystem/webossystem_injection.h"

#include <ctime>
#include <string>

#include "base/bind.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/trace_event/neva/lttng/webos/pmtracer.h"
#include "base/values.h"
#include "gin/arguments.h"
#include "gin/dictionary.h"
#include "gin/function_template.h"
#include "gin/handle.h"
#include "gin/object_template_builder.h"
#include "gin/wrappable.h"
#include "neva/injection/common/gin/function_template_neva.h"
#include "neva/injection/common/public/renderer/injection_webos.h"
#include "neva/injection/common/webos/luna_service_mgr.h"
#include "neva/injection/webosservicebridge/webos_servicebridge_injection.h"
#include "neva/injection/webossystem/webossystem_datamanager.h"
#include "neva/injection/grit/injection_resources.h"
#include "third_party/blink/public/web/blink.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_script_source.h"
#include "ui/base/resource/resource_bundle.h"

namespace injections {

class CursorInjection : public gin::Wrappable<CursorInjection> {
 public:
  static gin::WrapperInfo kWrapperInfo;

  class Delegate {
   public:
    virtual std::string CallFunctionName(const std::string& name) = 0;
    virtual bool SetCursor(gin::Arguments* args) = 0;
  };

  CursorInjection(Delegate* delegate);
  ~CursorInjection() = default;

 private:
  // gin::Wrappable.
  gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) final;

  bool GetCursorVisibility();
  std::string GetCursorState();
  bool SetCursor(gin::Arguments* args);

  Delegate* delegate_;
};

CursorInjection::CursorInjection(Delegate* delegate)
    : delegate_(delegate) {}

gin::WrapperInfo CursorInjection::kWrapperInfo = {gin::kEmbedderNativeGin};
gin::ObjectTemplateBuilder CursorInjection::GetObjectTemplateBuilder(
    v8::Isolate* isolate) {
  return gin::Wrappable<CursorInjection>::GetObjectTemplateBuilder(isolate)
    .SetProperty("visibility", &CursorInjection::GetCursorVisibility)
    .SetMethod("getCursorState", &CursorInjection::GetCursorState)
    .SetMethod("setCursor", &CursorInjection::SetCursor);
}

bool CursorInjection::GetCursorVisibility() {
  return delegate_->CallFunctionName("cursorVisibility") == "true";
}

std::string CursorInjection::GetCursorState() {
  return "{ \"visibility\" : " +
         delegate_->CallFunctionName("cursorVisibility") +
         " }";
}

bool CursorInjection::SetCursor(gin::Arguments* args) {
  return delegate_->SetCursor(args);
}

class WindowInjection : public gin::Wrappable<WindowInjection> {
 public:
  static gin::WrapperInfo kWrapperInfo;

  class Delegate {
   public:
    virtual void SetInputRegion(gin::Arguments* args) = 0;
    virtual void SetWindowProperty(
        const std::string& arg1, const std::string& arg2) = 0;
  };

  WindowInjection(Delegate* delegate);
  ~WindowInjection() = default;

 private:
  // gin::Wrappable.
  gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) final;

  void SetInputRegion(gin::Arguments* args);
  void SetProperty(const std::string& arg1, const std::string& arg2);

  Delegate* delegate_;
};

WindowInjection::WindowInjection(Delegate* delegate)
    : delegate_(delegate) {}

gin::WrapperInfo WindowInjection::kWrapperInfo = {gin::kEmbedderNativeGin};
gin::ObjectTemplateBuilder WindowInjection::GetObjectTemplateBuilder(
    v8::Isolate* isolate) {
  return gin::Wrappable<WindowInjection>::GetObjectTemplateBuilder(isolate)
    .SetMethod("setInputRegion", &WindowInjection::SetInputRegion)
    .SetMethod("setProperty", &WindowInjection::SetProperty);
}

void WindowInjection::SetInputRegion(gin::Arguments* args) {
  delegate_->SetInputRegion(args);
}

void WindowInjection::SetProperty(
    const std::string& arg1, const std::string& arg2) {
  delegate_->SetWindowProperty(arg1, arg2);
}


class WebOSSystemInjection : public gin::Wrappable<WebOSSystemInjection>,
                             public InjectionWebOS,
                             public WindowInjection::Delegate,
                             public CursorInjection::Delegate {
 public:
  static gin::WrapperInfo kWrapperInfo;

  WebOSSystemInjection();
  ~WebOSSystemInjection() override;

  void BuildExtraObjects(v8::Local<v8::Object> obj,
                         v8::Isolate* isolate,
                         v8::Local<v8::Context> context);

  // Override WindowInjection::Delegate
  void SetInputRegion(gin::Arguments* args) override;
  void SetWindowProperty(
      const std::string& arg1, const std::string& arg2) override;
  // Override CursorInjection::Delegate
  std::string CallFunctionName(const std::string& name) override;
  bool SetCursor(gin::Arguments* args) override;

 private:
  // gin::Wrappable.
  gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) final;

  // Handlers for JS properties.
  std::string GetResource(const std::string& arg);
  double DevicePixelRatio();
  std::string GetCountry();
  bool GetIsMinimal();
  std::string GetActivityId();
  std::string GetDeviceInfo();
  std::string GetTimeZone();
  std::string GetTimeFormat();
  std::string GetIdentifier();
  std::string GetLocale();
  std::string GetLocaleRegion();
  std::string GetPhoneRegion();
  std::string GetScreenOrientation();
  bool GetIsActivated();
  std::string GetLaunchParams();
  void SetLaunchParams(const std::string& launch_params);
  std::string GetWindowOrientation();
  void SetWindowOrientation(const std::string& param);
  bool GetIsKeyboardVisible();
  void Activate();
  void Deactivate();
  void OnCloseNotify(const std::string& param);
  void NativePmLogInfoWithClock(gin::Arguments* args);
  void NativePmLogString(gin::Arguments* args);
  void NativePmTrace(const std::string& param);
  void NativePmTraceItem(const std::string& param0, const std::string& param1);
  void NativePmTraceBefore(const std::string& param);
  void NativePmTraceAfter(const std::string& param);
  void AddBannerMessage(gin::Arguments* args);
  void RemoveBannerMessage(const std::string& param);
  void ClearBannerMessages();
  void SimulateMouseClick(const std::string& result0,
                          const std::string& result1,
                          bool result2);
  void UseSimulatedMouseClicks(bool param);
  void Paste();
  void CopiedToClipboard();
  void PastedFromClipboard();
  void MarkFirstUseDone();
  void EnableFullScreenMode(bool param);
  void StagePreparing();
  void StageReady();
  void ContainerReady();
  void EditorFocused();
  void KeepAlive(bool param);
  void ApplyLaunchFeedback();
  std::string AddNewContentIndicator();
  void RemoveNewContentIndicator(const std::string& param);
  void KeyboardShow(int param);
  void KeyboardHide();
  void SetManualKeyboardEnabled(bool param);
  void PlatformBack();
  void SetKeyMask(const std::vector<std::string>& str_array);
  void SetLoadErrorPolicy(const std::string& param);
  void Hide();
  void FocusOwner();
  void FocusLayer();
  std::string ServiceCall(const std::string& uri, const std::string& payload);
  void SetAppInClosing();
  bool DidRunOnCloseCallback();
  void UpdateInjectionData(const std::string& key, const std::string& value);
  void ReloadInjectionData();

  std::string GetInjectionData(const std::string& name);

  WebOSSystemDataManager* data_manager_;

  DISALLOW_COPY_AND_ASSIGN(WebOSSystemInjection);
};

gin::WrapperInfo WebOSSystemInjection::kWrapperInfo = {gin::kEmbedderNativeGin};

WebOSSystemInjection::WebOSSystemInjection() {
  data_manager_ = new WebOSSystemDataManager(CallFunction("initialize"));
}

WebOSSystemInjection::~WebOSSystemInjection() {
  if (data_manager_)
    delete data_manager_;
}

void WebOSSystemInjection::BuildExtraObjects(v8::Local<v8::Object> obj,
                                             v8::Isolate* isolate,
                                             v8::Local<v8::Context> context) {
  // Build webOSSystem.window
  gin::Handle<WindowInjection> window_obj =
    gin::CreateHandle(isolate, new WindowInjection(this));
  obj->Set(gin::StringToV8(isolate, "window"), window_obj.ToV8());

  // Build webOSSystem.cursor
  gin::Handle<CursorInjection> cursor_obj =
    gin::CreateHandle(isolate, new CursorInjection(this));
  obj->Set(gin::StringToV8(isolate, "cursor"), cursor_obj.ToV8());

  const std::string extra_objects_js(
      ui::ResourceBundle::GetSharedInstance().GetRawDataResource(
          IDR_WEBOSSYSTEM_INJECTION_JS));

  v8::Local<v8::Script> local_script;
  v8::MaybeLocal<v8::Script> script = v8::Script::Compile(
       context, gin::StringToV8(isolate, extra_objects_js.c_str()));
  if (script.ToLocal(&local_script))
    local_script->Run(context);
}

gin::ObjectTemplateBuilder WebOSSystemInjection::GetObjectTemplateBuilder(
    v8::Isolate* isolate) {
  return gin::Wrappable<WebOSSystemInjection>::GetObjectTemplateBuilder(isolate)
      .SetMethod("getResource", &WebOSSystemInjection::GetResource)
      .SetMethod("devicePixelRatio", &WebOSSystemInjection::DevicePixelRatio)
       /* for caching */
      .SetProperty("country", &WebOSSystemInjection::GetCountry)
      .SetProperty("isMinimal", &WebOSSystemInjection::GetIsMinimal)
      .SetProperty("activityId", &WebOSSystemInjection::GetActivityId)
      .SetProperty("deviceInfo", &WebOSSystemInjection::GetDeviceInfo)
      .SetProperty("timeZone", &WebOSSystemInjection::GetTimeZone)
      .SetProperty("timeFormat", &WebOSSystemInjection::GetTimeFormat)
      .SetProperty("identifier", &WebOSSystemInjection::GetIdentifier)
      .SetProperty("locale", &WebOSSystemInjection::GetLocale)
      .SetProperty("localeRegion", &WebOSSystemInjection::GetLocaleRegion)
      .SetProperty("phoneRegion", &WebOSSystemInjection::GetPhoneRegion)
      .SetProperty("screenOrientation",
                                  &WebOSSystemInjection::GetScreenOrientation)
      .SetProperty("isActivated", &WebOSSystemInjection::GetIsActivated)
       /* getter, setter */
      .SetProperty("launchParams", &WebOSSystemInjection::GetLaunchParams,
                                    &WebOSSystemInjection::SetLaunchParams)
      .SetProperty("windowOrientation",
                    &WebOSSystemInjection::GetWindowOrientation,
                    &WebOSSystemInjection::SetWindowOrientation)
       /* not for caching */
      .SetProperty("isKeyboardVisible",
                                  &WebOSSystemInjection::GetIsKeyboardVisible)
      /* setMethod */
      .SetMethod("activate", &WebOSSystemInjection::Activate)
      .SetMethod("deactivate", &WebOSSystemInjection::Deactivate)
      .SetMethod("onCloseNotify", &WebOSSystemInjection::OnCloseNotify)
      .SetMethod("getIdentifier", &WebOSSystemInjection::GetIdentifier)
      .SetMethod("setWindowOrientation",
                                  &WebOSSystemInjection::SetWindowOrientation)
      .SetMethod("PmLogInfoWithClock",
                              &WebOSSystemInjection::NativePmLogInfoWithClock)
      .SetMethod("PmLogString", &WebOSSystemInjection::NativePmLogString)
      .SetMethod("PmTrace", &WebOSSystemInjection::NativePmTrace)
      .SetMethod("PmTraceItem", &WebOSSystemInjection::NativePmTraceItem)
      .SetMethod("PmTraceBefore", &WebOSSystemInjection::NativePmTraceBefore)
      .SetMethod("PmTraceAfter", &WebOSSystemInjection::NativePmTraceAfter)
      .SetMethod("addBannerMessage", &WebOSSystemInjection::AddBannerMessage)
      .SetMethod("removeBannerMessage",
                                  &WebOSSystemInjection::RemoveBannerMessage)
      .SetMethod("clearBannerMessages",
                                  &WebOSSystemInjection::ClearBannerMessages)
      .SetMethod("simulateMouseClick",
                                  &WebOSSystemInjection::SimulateMouseClick)
      .SetMethod("useSimulatedMouseClicks",
                                  &WebOSSystemInjection::UseSimulatedMouseClicks)
      .SetMethod("paste", &WebOSSystemInjection::Paste)
      .SetMethod("copiedToClipboard", &WebOSSystemInjection::CopiedToClipboard)
      .SetMethod("pastedFromClipboard",
                                  &WebOSSystemInjection::PastedFromClipboard)
      .SetMethod("markFirstUseDone", &WebOSSystemInjection::MarkFirstUseDone)
      .SetMethod("enableFullScreenMode",
                                  &WebOSSystemInjection::EnableFullScreenMode)
      .SetMethod("stagePreparing", &WebOSSystemInjection::StagePreparing)
      .SetMethod("stageReady", &WebOSSystemInjection::StageReady)
      .SetMethod("containerReady", &WebOSSystemInjection::ContainerReady)
      .SetMethod("editorFocused", &WebOSSystemInjection::EditorFocused)
      .SetMethod("keepAlive", &WebOSSystemInjection::KeepAlive)
      .SetMethod("applyLaunchFeedback",
                                   &WebOSSystemInjection::ApplyLaunchFeedback)
      .SetMethod("addNewContentIndicator",
                             &WebOSSystemInjection::AddNewContentIndicator)
      .SetMethod("removeNewContentIndicator",
                             &WebOSSystemInjection::RemoveNewContentIndicator)
      .SetMethod("keyboardShow", &WebOSSystemInjection::KeyboardShow)
      .SetMethod("keyboardHide", &WebOSSystemInjection::KeyboardHide)
      .SetMethod("setManualKeyboardEnabled",
                             &WebOSSystemInjection::SetManualKeyboardEnabled)
      .SetMethod("platformBack", &WebOSSystemInjection::PlatformBack)
      .SetMethod("setKeyMask", &WebOSSystemInjection::SetKeyMask)
      .SetMethod("setLoadErrorPolicy", &WebOSSystemInjection::SetLoadErrorPolicy)
      .SetMethod("hide", &WebOSSystemInjection::Hide)
      .SetMethod("focusOwner", &WebOSSystemInjection::FocusOwner)
      .SetMethod("focusLayer", &WebOSSystemInjection::FocusLayer)
      .SetMethod("setInputRegion", &WebOSSystemInjection::SetInputRegion)
      .SetMethod("setWindowProperty", &WebOSSystemInjection::SetWindowProperty)
      .SetMethod("setCursor", &WebOSSystemInjection::SetCursor)
      .SetMethod("_setAppInClosing_", &WebOSSystemInjection::SetAppInClosing)
      .SetMethod("_didRunOnCloseCallback_", &WebOSSystemInjection::DidRunOnCloseCallback)
      .SetMethod("updateInjectionData", &WebOSSystemInjection::UpdateInjectionData)
      .SetMethod("reloadInjectionData", &WebOSSystemInjection::ReloadInjectionData)
      .SetMethod("serviceCall", &WebOSSystemInjection::ServiceCall)
  ;
}

void WebOSSystemInjection::ReloadInjectionData() {
  data_manager_->SetInitializedStatus(false);
  data_manager_->DoInitialize(CallFunction("initialize"));
}

void WebOSSystemInjection::UpdateInjectionData(
    const std::string& key, const std::string& value) {
  data_manager_->UpdateInjectionData(key, value);
}

bool WebOSSystemInjection::DidRunOnCloseCallback() {
  return WebOSServiceBridgeInjection::waiting_responses_.empty();
}

void WebOSSystemInjection::SetAppInClosing() {
  // Even if app A set this class static variable to true
  // This does not affect to other apps
  WebOSServiceBridgeInjection::is_closing_ = true;
}

bool WebOSSystemInjection::SetCursor(gin::Arguments* args) {
  std::string cursor_arg;
  if (!args->GetNext(&cursor_arg))
    return false;

  std::string x = "0";
  std::string y = "0";
  int arg2;
  if (args->GetNext(&arg2)) {
    x = std::to_string(arg2);
    int arg3;
    if (args->GetNext(&arg3))
      y = std::to_string(arg3);
  }

  if (cursor_arg.empty())
    cursor_arg = "default";
  else if (cursor_arg != "default" && cursor_arg != "blank") {
    //custom cursor : need to check file path
    cursor_arg = checkFileValidation(cursor_arg, GetInjectionData("folderPath"));
    if (cursor_arg.empty())
      return false;
  }

  std::vector<std::string> arguments = { cursor_arg, x, y };
  SendCommand("setCursor", arguments);

  return true;
}

std::string WebOSSystemInjection::CallFunctionName(const std::string& name) {
  return CallFunction(name);
}

void WebOSSystemInjection::SetWindowProperty(
    const std::string& arg1, const std::string& arg2) {
  std::vector<std::string> arguments = { arg1, arg2 };
  SendCommand("setWindowProperty", arguments);
}

void WebOSSystemInjection::SetInputRegion(gin::Arguments* args) {
  if (args->Length() == 0)
    return;

  v8::Isolate* isolate = args->isolate();
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Local<v8::Value> json_value = args->PeekNext();
  v8::MaybeLocal<v8::String> maybe_json_str = v8::JSON::Stringify(context, json_value);
  v8::Local<v8::String> json_str;
  if (maybe_json_str.ToLocal(&json_str)) {
    std::string json = gin::V8ToString(args->isolate(), json_str);
    std::vector<std::string> arguments = { json };
    SendCommand("setInputRegion", arguments);
  }
}

std::string WebOSSystemInjection::ServiceCall(
    const std::string& uri, const std::string& payload) {
  // check trustLevel : only trusted webapp can call this function
  if (GetInjectionData("trustLevel") != "trusted")
    return std::string("false");

  // This member is static class variable
  // But this value is separated between apps
  // TODO(pikulik): Send request only if application
  // is closing looks strange.
  if (!WebOSServiceBridgeInjection::is_closing_)
    return std::string("false");

  if (uri.empty() || payload.empty())
    return std::string("false");

  std::vector<std::string> arguments = { uri, payload };
  SendCommand("serviceCall", arguments);
  return std::string("true");
}

void WebOSSystemInjection::FocusLayer() {
  SendCommand("focusLayer");
}

void WebOSSystemInjection::FocusOwner() {
  SendCommand("focusOwner");
}

void WebOSSystemInjection::Hide() {
  SendCommand("hide");
}

void WebOSSystemInjection::SetLoadErrorPolicy(const std::string& param) {
  std::vector<std::string> arguments = { param };
  SendCommand("setLoadErrorPolicy", arguments);
}

void WebOSSystemInjection::SetKeyMask(const std::vector<std::string>& str_array) {
  base::ListValue root;
  for (const auto& item : str_array)
    root.AppendString(item);

  std::string output_js;
  base::JSONWriter::Write(root, &output_js);

  std::vector<std::string> arguments = { output_js };
  SendCommand("setKeyMask", arguments);
}

void WebOSSystemInjection::PlatformBack() {
  SendCommand("platformBack");
}

void WebOSSystemInjection::SetManualKeyboardEnabled(bool param) {
  std::vector<std::string> arguments;
  arguments.push_back(param ? "true" : "false");
  SendCommand("setManualKeyboardEnabled", arguments);
}

void WebOSSystemInjection::KeyboardHide() {
  SendCommand("keyboardHide");
}

void WebOSSystemInjection::KeyboardShow(int param) {
  std::vector<std::string> arguments = { std::to_string(param) };
  SendCommand("keyboardShow", arguments);
}

void WebOSSystemInjection::RemoveNewContentIndicator(
    const std::string& param) {
  std::vector<std::string> arguments = { param };
  SendCommand("removeNewContentIndicator", arguments);
}

std::string WebOSSystemInjection::AddNewContentIndicator() {
  return CallFunction("addNewContentIndicator");
}

void WebOSSystemInjection::ApplyLaunchFeedback() {
  //NOTIMPLEMENTED();
}

void WebOSSystemInjection::KeepAlive(bool param) {
  // Set websetting first because it is same renderer
  setKeepAliveWebApp(param);

  std::vector<std::string> arguments;
  arguments.push_back(param ? "true" : "false");
  SendCommand("keepAlive", arguments);
}

void WebOSSystemInjection::EditorFocused() {
  //NOTIMPLEMENTED();
}

void WebOSSystemInjection::ContainerReady() {
  SendCommand("containerReady");
}

void WebOSSystemInjection::StageReady() {
  SendCommand("stageReady");
}

void WebOSSystemInjection::StagePreparing() {
  SendCommand("stagePreparing");
}

void WebOSSystemInjection::EnableFullScreenMode(bool param){
  std::vector<std::string> arguments;
  arguments.push_back(param ? "true" : "false");
  SendCommand("enableFullScreenMode", arguments);
}

void WebOSSystemInjection::MarkFirstUseDone() {
  //NOTIMPLEMENTED();
}

void WebOSSystemInjection::PastedFromClipboard() {
  //NOTIMPLEMENTED();
}

void WebOSSystemInjection::CopiedToClipboard() {
  //NOTIMPLEMENTED();
}

void WebOSSystemInjection::Paste(){
  SendCommand("paste");
}

void WebOSSystemInjection::UseSimulatedMouseClicks(bool param) {
  std::vector<std::string> arguments;
  arguments.push_back(param ? "true" : "false");
  SendCommand("useSimulatedMouseClicks", arguments);
}

void WebOSSystemInjection::SimulateMouseClick(
    const std::string& result0, const std::string& result1, bool result2) {
  std::vector<std::string> arguments = { result0, result1 };
  arguments.push_back(result2 ? "true" : "false");
  SendCommand("simulateMouseClick", arguments);
}

void WebOSSystemInjection::ClearBannerMessages() {
  SendCommand("clearBannerMessages");
}

void WebOSSystemInjection::RemoveBannerMessage(const std::string& param) {
  std::vector<std::string> arguments = { param };
  SendCommand("removeBannerMessage", arguments);
}

void WebOSSystemInjection::AddBannerMessage(gin::Arguments* args) {
  if (args->Length() < 5)
    return;

  std::vector<std::string> arguments;
  for (auto x : args->GetAll())
    arguments.push_back(*v8::String::Utf8Value(blink::MainThreadIsolate(), x));

  if (arguments.size())
    args->Return(CallFunction("addBannerMessage", arguments));
}

void WebOSSystemInjection::NativePmTraceAfter(const std::string& param) {
  PMTRACE_AFTER(const_cast<char*>(param.c_str()));
}

void WebOSSystemInjection::NativePmTraceBefore(const std::string& param) {
  PMTRACE_BEFORE(const_cast<char*>(param.c_str()));
}

void WebOSSystemInjection::NativePmTraceItem(
    const std::string& param0, const std::string& param1) {
  char* param0_c_str = const_cast<char*>(param0.c_str());
  char* param1_c_str = const_cast<char*>(param1.c_str());
  PMTRACE_ITEM(param0_c_str, param1_c_str);
}

void WebOSSystemInjection::NativePmTrace(const std::string& param) {
  PMTRACE_LOG(const_cast<char*>(param.c_str()));
}

void WebOSSystemInjection::NativePmLogString(gin::Arguments* args) {
  if (args->Length() < 4)
    return;

  std::vector<std::string> arguments;
  for (auto x : args->GetAll())
    arguments.push_back(*v8::String::Utf8Value(blink::MainThreadIsolate(), x));

  if (arguments.size())
    SendCommand("PmLogString", arguments);
}

void WebOSSystemInjection::NativePmLogInfoWithClock(gin::Arguments* args) {
  if (args->Length() < 3)
    return;

  std::vector<std::string> arguments;
  for (auto x : args->GetAll())
    arguments.push_back(*v8::String::Utf8Value(blink::MainThreadIsolate(), x));

  if (arguments.size())
    SendCommand("PmLogInfoWithClock", arguments);
}

void WebOSSystemInjection::OnCloseNotify(const std::string& param) {
  // TODO(vladislav.mukulov): Migrate corresponded patch
  //RAW_PMLOG_INFO(
  // "[webOSSystem]", "webOSSystem.OnCloseNotify(%s)", param.c_str());
  std::vector<std::string> arguments = { param };
  SendCommand("onCloseNotify", arguments);
}

void WebOSSystemInjection::Deactivate() {
  SendCommand("deactivate");
}

void WebOSSystemInjection::Activate() {
  SendCommand("activate");
}

bool WebOSSystemInjection::GetIsKeyboardVisible() {
  return CallFunction("isKeyboardVisible") == "true";
}

std::string WebOSSystemInjection::GetWindowOrientation() {
  return GetInjectionData("windowOrientation");
}

void WebOSSystemInjection::SetWindowOrientation(const std::string& param){
  std::vector<std::string> arguments = { param };
  SendCommand("setWindowOrientation", arguments);
}

std::string WebOSSystemInjection::GetLaunchParams() {
  return GetInjectionData("launchParams");
}

void WebOSSystemInjection::SetLaunchParams(const std::string& launch_params) {
  if (data_manager_)
    data_manager_->UpdateInjectionData("launchParams", launch_params);

  std::vector<std::string> arguments = { launch_params };
  SendCommand("launchParams", arguments);
}

bool WebOSSystemInjection::GetIsActivated() {
  return GetInjectionData("isActivated") == "true";
}

std::string WebOSSystemInjection::GetScreenOrientation() {
  return GetInjectionData("screenOrientation");
}

std::string WebOSSystemInjection::GetPhoneRegion() {
  return GetInjectionData("phoneRegion");
}

std::string WebOSSystemInjection::GetLocaleRegion() {
  return GetInjectionData("localeRegion");
}

std::string WebOSSystemInjection::GetLocale() {
  return GetInjectionData("locale");
}

std::string WebOSSystemInjection::GetIdentifier() {
  return GetInjectionData("identifier");
}

std::string WebOSSystemInjection::GetTimeFormat() {
  return GetInjectionData("timeFormat");
}

std::string WebOSSystemInjection::GetTimeZone() {
  time_t localTime = time(0);
  tm localTM;
  localTM = *localtime(&localTime);

  return localTM.tm_zone? localTM.tm_zone: "";
}

std::string WebOSSystemInjection::GetDeviceInfo() {
  return GetInjectionData("deviceInfo");
}

std::string WebOSSystemInjection::GetActivityId() {
  return GetInjectionData("activityId");
}

bool WebOSSystemInjection::GetIsMinimal() {
  return GetInjectionData("isMinimal") == "true";
}

std::string WebOSSystemInjection::GetCountry() {
  return GetInjectionData("country");
}

std::string WebOSSystemInjection::GetInjectionData(const std::string& name) {
  if (!data_manager_->GetInitializedStatus())
    data_manager_->DoInitialize(CallFunction("initialize"));

  if (data_manager_->GetInitializedStatus()) {
    std::string result;
    if (data_manager_->GetInjectionData(name, result))
      return result;
  }
  return CallFunction(name);
}

std::string WebOSSystemInjection::GetResource(const std::string& arg) {
  std::vector<std::string> arguments = { arg };
  return CallFunction("getResource", arguments);
}

double WebOSSystemInjection::DevicePixelRatio() {
  return std::stod(GetInjectionData("devicePixelRatio"));
}

const char WebOSSystemInjectionExtension::kInjectionName[] = "v8/webossystem";
const char WebOSSystemInjectionExtension::kObsoleteName[] = "v8/palmsystem";

void WebOSSystemInjectionExtension::Install(blink::WebLocalFrame* frame) {
  v8::Isolate* isolate = blink::MainThreadIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = frame->MainWorldScriptContext();
  if (context.IsEmpty())
    return;

  v8::Local<v8::Object> global = context->Global();
  v8::Context::Scope context_scope(context);
  v8::Local<v8::Value> webossystem_value =
      global->Get(gin::StringToV8(isolate, "webOSSystem"));

  if (!webossystem_value.IsEmpty() && webossystem_value->IsObject())
    return;

  v8::MaybeLocal<v8::Object> webossystem =
      CreateWebOSSystemObject(isolate, global);
  v8::Local<v8::Object> local_webossystem;
  if (!webossystem.ToLocal(&local_webossystem))
    return;

  WebOSSystemInjection* webossystem_injection = nullptr;
  gin::Converter<WebOSSystemInjection*>::FromV8(
      isolate, local_webossystem, &webossystem_injection);
  webossystem_injection->BuildExtraObjects(local_webossystem, isolate, context);
}

void WebOSSystemInjectionExtension:: Uninstall(blink::WebLocalFrame* frame) {
  const std::string extra_objects_js(
      ui::ResourceBundle::GetSharedInstance().GetRawDataResource(
          IDR_WEBOSSYSTEM_ROLLBACK_JS));

  v8::Isolate* isolate = blink::MainThreadIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = frame->MainWorldScriptContext();
  if (context.IsEmpty())
    return;

  v8::Context::Scope context_scope(context);
  v8::Local<v8::Script> local_script;
  v8::MaybeLocal<v8::Script> script = v8::Script::Compile(
      context,
      gin::StringToV8(isolate, extra_objects_js.c_str()));

  if (script.ToLocal(&local_script))
    local_script->Run(context);
}

v8::MaybeLocal<v8::Object>
WebOSSystemInjectionExtension::CreateWebOSSystemObject(
    v8::Isolate* isolate,
    v8::Local<v8::Object> global) {
  gin::Handle<WebOSSystemInjection> webossystem =
      gin::CreateHandle(isolate, new WebOSSystemInjection());
  global->Set(gin::StringToV8(isolate, "webOSSystem"), webossystem.ToV8());
  return webossystem->GetWrapper(isolate);
}

}  // namespace injections
