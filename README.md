# 共享屏幕（v1.0.0）
这个项目启发自：[scrcpy](https://github.com/Genymobile/scrcpy)，把安卓屏幕共享到电脑上。
和scrcpy不同的是，我们在安卓平台上通过app运行(安卓5.0开始开放录屏权限)，而不是通过adb连接。
所以只有屏幕是共享的，无法模拟触屏点击事件（安卓系统没有开放此权限）。

# 特点
- 低延迟（~50ms）。
- 自动连接，安卓手机和电脑在同一网络，双方开始运行即会自动连接。
- 完全开源，免费、安全、干净。

# 测试的系统
安卓端：
- Android/8.0（荣耀 6x）
- Android/11.0（红米 note 10 pro）

所以期待能运行在Android8.0及以上系统。

电脑端：
- Windows
    - win7/64位
    - win10/64位
- Linux
    - ubuntu-18.04/64位
    - ubuntu-22.04/64位
- Macos
    - 10.13.6/64位

PS：个人精力有限，32位系统不打算测试，默认算不支持（但是代码可能可以编译）。

# 如何使用
电脑端：直接双击运行share_screen即可，也可以运行run.bat/run.sh。

安卓端：点击开始按钮即可。

# 如何编译项目
电脑端：

准备
- windows：visual studio，linux：gcc，macos：xcode，支持c++17即可。
- Cmake > 3.20。
- 编译ffmpeg，源码包位于`share_screen/pc/3rd/ffmpeg-5.1.1.tar.bz2`，具体参考：[ffmpeg](./doc/ffmpeg.md)。

唯一外部依赖是ffmpeg，需要指定FFMPEG_INSTALL_PATH，例如：`cmake -DFFMPEG_INSTALL_PATH=...`，直接用cmake生成项目文件即可。

安卓端：没有任何依赖，直接用Android Studio打开`share_screen/android`文件夹编译即可。

# 已知问题

# 发布历史
- 1.0.0
    - 主体完成。

# 许可证（MIT）
MIT License

Copyright (c) 2024 llcxm(llcxiongmao@163.com)

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.