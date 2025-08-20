基于 Win32 使用 WebView2
==========

## [Win32 应用中的 WebView2 入门](https://learn.microsoft.com/zh-cn/microsoft-edge/webview2/get-started/win32#step-8---update-or-install-the-webview2-sdk)

[Get started with WebView2 in Win32 apps](https://github.com/MicrosoftEdge/WebView2Samples/tree/main/GettingStartedGuides/Win32_GettingStarted)

## NuGet CLI

[NuGet 下载](https://www.nuget.org/downloads)
[install command (NuGet CLI)](https://learn.microsoft.com/en-us/nuget/reference/cli-reference/cli-ref-install)

## WebView2 与 C++ 之间的桥接代码

基于 WebView2 的 API 进行的简单封装

[resource/bridge.js](resource/bridge.js) 的内容直接拷贝到C++中通过
`webview->AddScriptToExecuteOnDocumentCreated(bridgeScript, nullptr);`加载

## 消息协议

### HTML ----> Native (请求)

```
{ "id": "<string>", "cmd": "<string>", "args": <any> }
```

JS

```
await Native.invoke("activateWindow", { handle: handle });
```

### Native ----> HTML (相应)

```
{ "id": "<same>", "result": <any> }
```

C++

```
void WebViewController::sendResult(const std::string &id, const nlohmann::json &result) {
    nlohmann::json payload = {
        {"id", id},
        {"result", result}
    };
    webview->PostWebMessageAsJson(Utils::StringToWString(payload.dump()).c_str());
}
```

### Native ----> HTML (事件)

```
{ "event": "<string>", "data": <any> }
```

JS

```
Native.on('tick', (data)=> log('tick:', JSON.stringify(data)));
```

C++

```
void WebViewController::emitEvent(const std::string &name, const nlohmann::json &data) {
    nlohmann::json payload = {
        {"event", name},
        {"data", data}
    };
    webview->PostWebMessageAsJson(Utils::StringToWString(payload.dump()).c_str());
}
```
