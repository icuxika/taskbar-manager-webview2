# Build.ps1 构建脚本说明文档

## 概述

`Build.ps1` 是一个 PowerShell 构建脚本，用于自动化构建 TaskbarManager 应用程序。它支持两种构建类型：ZIP 应用程序映像和 NSIS 安装程序包，并可选择包含必要的运行时依赖。

## 参数说明

### 必需参数

- **`-Type`** (必需)
  - 描述：指定构建类型
  - 可选值：`ZIP` 或 `NSIS`
  - `ZIP`: 生成应用程序映像，用户通过解压 ZIP 文件获得程序
  - `NSIS`: 生成安装程序包，用户通过安装向导安装程序

### 可选参数

- **`-IncludeVCRedist`** (可选开关)
  - 描述：是否包含 Microsoft Visual C++ 可再发行程序包
  - 默认：不包含
  - 启用后会自动下载 VC++ redist 并包含在构建产物中

- **`-IncludeWebView2Runtime`** (可选开关)
  - 描述：是否包含 WebView2 Runtime
  - 默认：不包含
  - 启用后会自动下载 WebView2 Runtime 并包含在构建产物中

## 使用示例

### 基本构建（仅应用程序）
```powershell
.\Build.ps1 -Type ZIP
.\Build.ps1 -Type NSIS
```

### 包含 VC++ 可再发行程序包
```powershell
.\Build.ps1 -Type ZIP -IncludeVCRedist
.\Build.ps1 -Type NSIS -IncludeVCRedist
```

### 包含 WebView2 Runtime
```powershell
.\Build.ps1 -Type ZIP -IncludeWebView2Runtime
.\Build.ps1 -Type NSIS -IncludeWebView2Runtime
```

### 包含所有运行时依赖
```powershell
.\Build.ps1 -Type ZIP -IncludeVCRedist -IncludeWebView2Runtime
.\Build.ps1 -Type NSIS -IncludeVCRedist -IncludeWebView2Runtime
```

## 构建流程

### 1. 准备阶段
- 检查并下载必要的依赖文件（如果指定了相关开关）
- VC++ redist: 从 Microsoft 官方服务器下载
- WebView2 Runtime: 从 Microsoft 官方服务器下载

### 2. 构建阶段
- 使用 CMake 配置项目
- 使用 CMake 编译项目（MinSizeRel 配置）
- 使用 CPack 打包基础应用程序

### 3. 打包阶段（根据构建类型）

#### ZIP 类型
- 解压基础应用程序包
- 根据选择的依赖选项添加相应文件
- 重新压缩为最终的 ZIP 包
- 生成的文件命名规则：
  - 基础包: `TaskbarManager-{version}-win64.zip`
  - 包含 VC++ redist: `TaskbarManager-{version}-win64-VC_redist.zip`
  - 包含 WebView2 Runtime: `TaskbarManager-{version}-win64-WebView2_Runtime.zip`
  - 包含两者: `TaskbarManager-{version}-win64-VC_redist-WebView2_Runtime.zip`

#### NSIS 类型
- 准备 NSIS 安装程序源目录
- 根据选择的依赖选项添加相应文件到安装程序源
- 调用 NSIS 构建脚本生成安装程序
- 最终安装程序文件: `nsis/Install TaskbarManager.exe`

## 文件位置

- **构建产物**: 项目根目录（与脚本同级的父目录）
- **依赖缓存**: `script/` 目录下
  - VC++ redist: `VC_redist.x64.exe`
  - WebView2 Runtime: `Microsoft.WebView2.FixedVersionRuntime.cab`
- **临时文件**: 构建过程中会在 `script/` 目录创建临时文件夹
