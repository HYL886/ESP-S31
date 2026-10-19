# ESP-S31：ESP32-S31-Korvo-1 屏幕与联网学习工程

这是一个完整的 ESP-IDF 应用工程，使用官方 BSP 1.0.1 和 LVGL 9.3.0。当前包含自定义标题、加 10 按钮、清零按钮、每秒刷新的网络时钟，以及自动扫描并点选热点的屏幕 Wi-Fi 配网页。

开发环境为本电脑的 ESP-IDF master（v6.2.0）。请先阅读 [Wi-Fi 时钟改动与操作说明](WIFI_CLOCK.md)。烧录后点击屏幕右上角 **网络**，等待扫描、点选热点并输入密码；连接成功后保存，重启自动连接。换网络无须重新编译。

## 打开与烧录

GitHub 仓库：[HYL886/ESP-S31](https://github.com/HYL886/ESP-S31)。在新电脑上先获取源码：

```powershell
git clone https://github.com/HYL886/ESP-S31.git
cd ESP-S31
code .
```

新电脑获取源码后，直接用 VS Code 打开仓库根目录。使用 ESP-IDF 扩展选择本机安装的、支持 esp32s31 的 IDF 环境和实际串口，扩展会管理本机设置。首次构建会自动获取依赖；Wi-Fi 名称由屏幕列表选择，密码通过屏幕输入。

中文点阵字库已包含在 `main/fonts` 并直接参与编译，克隆后无需安装字体转换工具或额外开启旧 CJK 字体选项。24 像素字体覆盖 GB2312 的 6763 个常用汉字及当前界面标点；16 像素用于状态提示，32 像素用于计数和按钮。中文界面与中文热点名称均使用这些字体。

1. 在 VS Code 中“文件 → 打开文件夹”，打开源码根目录（本机为 `s31_display`，克隆后默认是 `ESP-S31`），应能看到顶层 `CMakeLists.txt`、`main` 文件夹和 `sdkconfig.defaults`。
   本机也可以选择“文件 → 从文件打开工作区”，打开本目录外面的 `s31_display.code-workspace`；这个便捷工作区文件不属于仓库源码。
2. 执行 `ESP-IDF: Select Current ESP-IDF Version`，选择电脑已有的 `master`，目录为 `C:\esp\master\esp-idf`。
3. 将数据线连接开发板的 UART Type-C 口，保持屏幕子板连接，打开板上电源。当前串口配置为 COM11；换接口后以设备管理器显示的串口为准。
4. 先在之前的串口监视器按 `Ctrl + ]` 退出，释放串口。
5. 打开 ESP-IDF 终端，在本工程目录运行：

```powershell
idf.py -p COM11 build flash monitor
```

首次构建会自动获取 BSP、LVGL 等依赖。此操作会把板上当前程序替换成新的屏幕与网络时钟程序。

## 预期结果

- 保留自定义标题 `My first screen`，计数显示 `计数：0`。
- 点击 `加 10` 按钮，数字加 10；点击 `清零`，数字清零。
- 在屏幕上配置 Wi-Fi 并校时后，副标题显示北京时间，每秒刷新。
- `网络` 页面自动扫描附近 2.4 GHz 热点，按信号强弱排序；点选后只需填写密码。`重新搜索` 刷新列表，`热点列表` 返回列表，`手动输入` 用于隐藏热点。
- 串口出现 `Network time synchronized.` 表示网络校时成功。
- 按 `Ctrl + ]` 退出串口监视器。

## 学习入口

打开 `main/main.c`：`create_screen()` 创建界面，两个按钮回调修改计数，`on_timer()` 每秒读取并显示时间。`main/wifi_setup.c` 创建热点列表和输入页；`main/wifi_clock.c` 通过后台任务处理扫描、连接、配置保存和校时；`main/wifi_networks.c` 负责扫描结果去重与排序。

`main/ui_fonts.c` 统一管理显示字体。屏幕键盘仍提供英文字母、数字和符号；扫描到的中文 SSID 会原样用于连接，字库覆盖范围外的字符可能显示为方框。字体来源、授权和重新生成步骤详见 [WIFI_CLOCK.md](WIFI_CLOCK.md)。

## 参考

- [开发板官方中文指南](https://docs.espressif.com/projects/esp-dev-kits/zh_CN/latest/esp32s31/esp32-s31-korvo-1/user_guide.html)
- [官方最小显示例程](https://github.com/espressif/esp-bsp/tree/master/examples/display)
- [官方 BSP 1.0.1](https://components.espressif.com/components/espressif/esp32_s31_korvo_1/versions/1.0.1/readme)

## 本机校验测试

安装有 GCC 的电脑可在仓库根目录运行以下命令。该测试不需要连接开发板：

```powershell
gcc -std=c11 -Wall -Wextra -Werror -I main tests/wifi_credentials_test.c main/wifi_credentials.c -o wifi_credentials_test.exe
.\wifi_credentials_test.exe
gcc -std=c11 -Wall -Wextra -Werror -I main tests/wifi_networks_test.c main/wifi_networks.c -o wifi_networks_test.exe
.\wifi_networks_test.exe
python tests/font_coverage_test.py
```

测试程序只使用测试数据；第二组验证同名热点去重、信号排序、结果数量上限和中文名称保留；第三组直接检查生成的字体数据，确认所有中文提示和 6763 个常用汉字有有效点阵。真实扫描、显示、配网、保存及重启恢复需要在开发板上验证。
