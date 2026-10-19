# ESP-S31 学习笔记：Wi-Fi 时钟、中文界面与摄像头

## 本次改了什么

最初把 Wi-Fi 名称和密码写在头文件中，换网络需要重新编译；随后改为屏幕输入、自动扫描和中文界面。当前新增 OV3660 摄像头实时预览，主界面右上角点击“摄像头”进入。下面先保留网络与字体课程的完整说明，摄像头的全部改动、配置、原理及验证步骤见“摄像头实时预览：本次全部改动”。

| 文件 | 具体改动 |
| --- | --- |
| main/main.c | 主界面右上角为“网络”按钮；计时回调刷新配网页状态，并提示首次配网或连接失败。保留已修改的英文标题，加数和清零按钮显示中文。 |
| main/wifi_setup.c / wifi_setup.h | 配网页拆成热点列表和输入页；自动扫描、Rescan 重扫、点选名称后输入密码，Manual name 供隐藏热点使用。 |
| main/wifi_clock.c / wifi_clock.h | 删除对 wifi_config.h 的依赖；通过队列接收屏幕输入。独立任务管理停止旧连接、切换网络、重试、校时与 NVS 读写；界面只读取不含密码的状态快照。 |
| main/wifi_credentials.c / wifi_credentials.h | 新增输入校验：SSID 按 UTF-8 字节计数，不超过 32 字节；密码为 8–63 字节，或 64 位十六进制 PSK；空密码表示开放网络。 |
| main/wifi_networks.c / wifi_networks.h | 新增扫描结果数据结构、按名称/安全类型去重、保留较强信号、按 RSSI 排序及最多 20 项的限制。 |
| main/CMakeLists.txt | 加入配网页及校验源文件，并显式声明网络、定时器与 NVS 组件依赖。 |
| tests/wifi_credentials_test.c | 增加名称长度、UTF-8 字节数、空密码、短密码、64 位 PSK 等边界测试。 |
| tests/wifi_networks_test.c | 验证重复热点、不同安全类型、强弱信号、中文/32 字节 SSID、空名称及结果容量边界。 |
| main/ui_fonts.c / ui_fonts.h | 统一提供 16、24、32 像素字体；在创建界面前初始化，英文与 LVGL 符号沿用 Montserrat，中文使用本工程点阵字库。 |
| main/fonts/s31_cn_16.c、s31_cn_24.c、s31_cn_32.c | 实际编译使用的中文点阵数据。24 像素覆盖 GB2312 常用汉字，16/32 像素按界面实际文字生成，减少固件体积。 |
| main/fonts/OFL-SourceHanSans.txt | 保存字体来源的版权声明和 OFL 1.1 授权；该文件必须随字体保留。 |
| tools/generate_fonts.py / tests/font_coverage_test.py | 前者重新生成实际使用的字库，后者检查字体映射和点阵边界；普通编译无需运行转换脚本。 |
| sdkconfig.defaults | 保留实际使用的 Montserrat 字体选项，移除旧的 16 像素 CJK 备用字体，避免重复编入不再使用的字库。 |
| README.md | 更新为屏幕配网的入口和操作说明。 |

已删除停用的 main/wifi_config.h、main/wifi_config.example.h 和未被扩展读取的 .vscode/settings.example.json，也移除了没有调用者的 wifi_clock_is_connected() 便捷接口。现在不需要创建或填写任何 Wi-Fi 头文件；请直接在开发板上选网配网。本机正在使用的 .vscode/settings.json 保留，且不上传 GitHub。

## 现在怎么操作

当前按钮已显示为中文。下面保留的英文名称用于对应早期课程和事件函数，屏幕上的对应关系如下：

| 当前屏幕 | 早期名称 | 作用 |
| --- | --- | --- |
| 网络 | Wi-Fi | 打开配网页 |
| 加 10 / 清零 | Tap me +10 / Reset | 增加计数或清零 |
| 连接 / 返回 | Connect / Back | 提交连接或返回主界面 |
| 热点列表 / 重新搜索 | Networks / Rescan | 返回列表或刷新附近热点 |
| 手动输入 | Manual name | 配置隐藏热点 |

1. 在 VS Code 打开原来的 s31_display 工程，选择已有的 ESP-IDF master 环境。
2. 在旧串口监视器按 Ctrl + ] 退出，释放 COM11。
3. 在本工程 ESP-IDF 终端运行：

~~~powershell
idf.py -p COM11 build flash monitor
~~~

4. 在开发板主界面点击右上角 **网络**。
5. 等待附近 2.4 GHz 热点列表出现，点击自己的热点；名称已自动填好，键盘切到密码输入。
6. 填写“密码”，再点击 **连接**。开放网络可以留空；需要重新搜索时先点 **热点列表** 回列表，再点 **重新搜索**。
7. 看到 **已连接并保存** 后，点击 **返回** 回到时钟。网络校时完成后显示北京时间，每秒刷新。
8. 按板上的 RESET 重启，检查是否自动连接刚才保存的网络。

键盘上的确认键在填写 SSID 时切到密码框，在密码框时提交连接。Back 返回主界面；已经提交的连接请求仍在后台继续。再次打开配网页会先自动搜索，点选热点后密码框保持空白；重新点击 Connect 需要重新输入该网络密码。

隐藏热点不会出现在普通扫描列表中，请点 **手动输入** 填写。列表显示信号 dBm，越接近 0 通常信号越强，例如 -40 dBm 比 -80 dBm 强。相同名称与相同安全类型的多个接入点只保留较强的一项；开放和加密的同名热点会分开显示。标有“不支持的认证”的热点不可点选。

如果开发板正在连接已保存的网络，页面先显示等待，当前尝试成功或进入重试间隔后再扫描。扫描一般需要几秒，具体取决于附近无线环境；等待过程中仍可 Back 返回。扫描列表和输入页分开显示，避免列表挤占 800×480 屏幕上的密码键盘。

## 保存和失败处理

- **首次上电**：没有已保存网络时，主界面提示点击 Wi-Fi 进行配置。
- **保存时机**：新网络认证成功并取得 IP 地址后，才把名称和密码保存到 NVS。按 Connect 时不立即覆盖旧记录。
- **输错密码或找不到热点**：新网络最多重试 3 次，或约 30 秒超时；界面提示修改后重试。上一次成功保存的网络记录不被替换。
- **已保存网络暂时断开**：后台约每 5 秒发起重连；界面与计数按钮继续运行。
- **切换网络**：先停止旧连接，等待停止事件后再设置新网络，避免旧的断线/IP 事件误触发保存。
- **存储失败**：显示 Connected, but saving failed，表示本次已连上但未成功保存；可重新输入并点击 Connect 重试。
- **重新启动**：从 NVS 读取网络名称和密码，自动联网并开始校时。
- **校时成功后断网**：系统时钟继续走；完全断电后重新校时。
- 普通烧录保留现有 NVS；整片 erase-flash 或清空 NVS 会删除已保存的网络。

NVS 使用默认 nvs 分区，命名空间为 s31_wifi，键名为 station，保存带版本号的一个 blob。程序不会在初始化失败时自动整片擦除，也不会把密码写进日志或返回给界面状态查询。此学习工程未启用 NVS 加密。

应用分区仍为上一课设置的 3 MB；NVS 与 phy_init 的地址不变。

## 这一课理解什么

1. 屏幕输入框负责获取名称、密码；输入校验负责检查格式。
2. 队列把连接请求交给网络任务，界面回调不等待连接完成。
3. 网络事件推动“连接中 → 已连接 → 已保存”的状态变化。
4. NVS 将配置保留到下次启动。
5. time() 读取系统时间，localtime_r() 转换成本地日期，strftime() 格式化，LVGL 定时器每秒刷新标签。

网络校时使用 SNTP，服务器在 wifi_clock.c 中配置为 pool.ntp.org，时区为北京时间 UTC+8。如果已连接但持续等待校时，检查互联网连接和 NTP（UDP 123）是否可用。


## 从第一课到现在：完整改动顺序

这部分按我们实际完成练习的顺序整理。复习时先看一行，再打开对应源码确认效果，不必一次读完网络任务的全部代码。

| 阶段 | 原来怎样 | 改在哪里、改成什么 | 学到什么 |
| --- | --- | --- | --- |
| 0. 建立完整工程 | 单独打开桌面 BSP 包，扩展无法识别完整应用 | 新建根 CMakeLists.txt、main 目录和配置文件；主程序通过 BSP 使用屏幕 | 区分“板级组件”和“完整应用工程” |
| 1. 修改标题 | 显示 Hello ESP32-S31! | main.c 的 create_screen() 中改为 My first screen | 修改字符串、保存、重新编译与烧录 |
| 2. 改变步长 | 点击按钮加 1 | on_button_clicked() 改成 tap_count += 10，同时按钮文字改为 Tap me +10 | 变量是实际数据，按钮文字只是提示 |
| 3. 增加清零 | 只有加数按钮 | 新增 on_reset_clicked() 与 Reset 按钮，并把原按钮向左移 | 两个事件回调操作同一份状态 |
| 4. 自动更新 | 只有点击时改变界面 | 新增 LVGL 定时器，最初显示 lv_tick_get() / 1000 得到的运行秒数 | 定时回调与时间单位 |
| 5. 联网时钟 | 显示的是运行时长 | 加入 Wi-Fi、SNTP 和时区设置；on_timer() 改用系统时间 | 设备运行时长和真实日期时间的区别 |
| 6. 屏幕配网 | 名称、密码写进头文件，换网需要重新编译 | 新增输入框、屏幕键盘和网络任务；连接成功后保存 NVS | 运行时配置、任务通信和持久存储 |
| 7. 扫描选网与清理 | 每次都要手动输入名称，目录保留了停用模板 | 后台扫描、列表点选、信号排序和常用中文名称显示；删除停用文件与无调用接口 | 扫描结果整理、界面切换、后台操作和保持工程清晰 |
| 8. 中文字体与界面 | 只有热点名称有小字号中文备用字体，其他提示还是英文 | 新增实际编译的 16/24/32 像素点阵字库、统一字体接口和中文提示，保留自定义英文标题 | UTF-8 文本、字形覆盖、字体回退与 Flash 空间 |
| 9. 摄像头实时预览 | 摄像头驱动未开启，只有屏幕与网络功能 | 启用 OV3660 RGB565 模式，增加预览入口、后台采集、缩放旋转与安全退出 | V4L2 缓冲队列、双缓冲、字节序、任务同步与资源生命周期 |

### 工程文件一览：除了 C 代码，还改了什么

~~~text
s31_display/
├─ CMakeLists.txt                 完整工程入口，目标芯片 esp32s31
├─ sdkconfig.defaults             初始配置模板
├─ sdkconfig                      本机当前生成的配置
├─ partitions.csv                 分区表
├─ dependencies.lock              构建时锁定的实际依赖版本
├─ .gitignore                     忽略构建目录和旧私有配置文件
├─ .vscode/settings.json          本机 IDF、串口及目标配置
├─ main/
│  ├─ CMakeLists.txt              本应用需要编译的源文件
│  ├─ idf_component.yml           BSP、LVGL 和 IDF 版本要求
│  ├─ main.c                     主界面、按钮与时钟显示
│  ├─ wifi_setup.c / .h           屏幕配网页
│  ├─ wifi_clock.c / .h           网络后台任务、校时和保存
│  ├─ wifi_credentials.c / .h     输入校验和配置数据结构
│  ├─ wifi_networks.c / .h        扫描结果数据结构、去重和信号排序
│  ├─ camera_preview.c / .h       摄像头页面、后台采集和资源释放
│  ├─ camera_frame.c / .h         RGB565 字节序、缩放与方向转换
│  ├─ ui_fonts.c / .h            统一字体初始化和字号接口
│  └─ fonts/                    实际使用的点阵 C 文件和字体授权
├─ tests/
│  ├─ wifi_credentials_test.c     本机输入校验测试
│  ├─ wifi_networks_test.c        本机扫描结果整理测试
│  ├─ camera_frame_test.c         摄像头画面转换与边界测试
│  └─ font_coverage_test.py       本机字形覆盖和点阵边界检查
├─ tools/generate_fonts.py        修改文字后可重新生成字库
├─ README.md                      工程入口说明
└─ WIFI_CLOCK.md                  本文件：改动与学习笔记
~~~

工程外的 s31_display.code-workspace 是 VS Code 工作区入口，它指向 s31_display 文件夹。它不是固件的一部分。

| 文件或设置 | 当前作用 | 为什么需要 |
| --- | --- | --- |
| 根 CMakeLists.txt | 设置目标 esp32s31，引入 IDF 构建系统，启用最小依赖构建，声明工程名 s31_display | 让 VS Code 和 IDF 识别完整应用，不再把 BSP 组件当成应用 |
| main/idf_component.yml | 要求 IDF >= 6.1，使用 BSP 1.0.1 和 LVGL 9.3.0 | 组件管理器可以自动获取屏幕驱动与图形库 |
| dependencies.lock | 记录本次实际解析到的组件版本和哈希 | 方便复现当前构建环境 |
| sdkconfig.defaults | 固定 S31，启用 PSRAM、200 MHz PSRAM 时钟、双帧缓冲、防撕裂、1000 Hz FreeRTOS tick 和所需字体 | 屏幕帧缓冲需要外部内存，字体与显示功能需要在配置中启用 |
| .vscode/settings.json | 选择本机 master 环境、COM11、UART 下载和 S31 调试配置 | 减少每次打开工程时的重复选择 |
| .gitignore | 忽略 build、managed_components、生成的 sdkconfig 和旧 wifi_config.h | 将手写源码与本机缓存、旧私有文件区分开 |
| main/CMakeLists.txt | 注册界面、网络、输入校验、扫描结果整理、ui_fonts.c 和三个字体 C 文件，并声明组件依赖 | 只创建 .c 文件还不够，必须告诉构建系统把它编译进去 |

sdkconfig 是本机生成的实际配置，sdkconfig.defaults 是新建配置时使用的模板。已有 sdkconfig 时，改 defaults 通常不会覆盖已有选项；日常可通过 idf.py menuconfig 修改实际配置。

当前版本移除旧 CONFIG_LV_FONT_SOURCE_HAN_SANS_SC_16_CJK 选项，并关闭本机对应实际配置。中文字体作为 main/fonts 下的 C 文件直接编译，无需在 menuconfig 再选择旧 CJK 字体；24/32 像素 Montserrat 字体副本的 fallback 分别指向本工程同字号中文点阵，字体结构体统一由 ui_fonts.c 管理。

### 分区表为什么从默认应用分区改为 3 MB

原先最小屏幕程序可以放进默认的 1 MB 应用分区。加入 Wi-Fi 后，本机生成的应用固件已经超过 1 MB，因此增加了 partitions.csv，并把配置切换到自定义分区表。

| 名称 | 起始地址 | 大小 | 保存什么 |
| --- | --- | --- | --- |
| nvs | 0x9000 | 0x6000 | Wi-Fi 配置等持久数据 |
| phy_init | 0xf000 | 0x1000 | 射频初始化数据 |
| factory | 0x10000 | 3 MB | 当前应用固件 |

这次扩大的是应用区域，NVS 起始地址和大小沿用原工程。应用程序、NVS 配置和 RAM 变量是三类不同内容；重新刷应用代码不等于自动删除 NVS。

## 建议按这个顺序读源码

1. main/main.c：先理解屏幕上每个对象从哪里创建。
2. main/wifi_setup.c：再看一次 Connect 点击怎样读出输入内容。
3. main/wifi_credentials.c：理解输入如何被判断为有效或无效。
4. main/wifi_clock.h：先认识网络模块公开给界面的接口。
5. main/wifi_clock.c：沿一次连接流程读函数，不必从第一行逐字读到底。
6. tests/wifi_credentials_test.c：通过输入与预期结果检查自己的理解。

### main.c：入口、按钮和定时器

app_main() 的调用顺序是：

~~~text
bsp_display_start()
    ↓ 初始化显示、触摸和 LVGL
bsp_display_lock(0)
    ↓ 在启动任务中安全地创建界面
create_screen()
bsp_display_unlock()
    ↓ 创建结束
wifi_clock_start()
    ↓ 初始化网络模块并启动后台任务
app_main() 返回，LVGL 和网络任务继续工作
~~~

app_main() 返回后屏幕仍然运行，因为 LVGL 和网络逻辑都由各自任务持续处理，不需要自己写一个 while(1) 把 app_main() 挡住。

主要对象和变量：

| 名称 | 类型或作用 | 理解方式 |
| --- | --- | --- |
| tap_count | uint32_t，当前计数 | 程序真正保存的数值 |
| count_label | lv_obj_t 指针，计数标签 | 用来找到并更新屏幕上的文字对象 |
| title | 标题标签 | 改文字不改变计数 |
| subtitle | 时间或网络提示标签 | 交给定时器每秒更新 |
| button / reset_button | 加数和清零按钮 | 创建按钮之后，还要绑定点击事件 |
| wifi_button | 右上角配网入口 | 点击时调用 wifi_setup_open() |
| footer | 底部操作提示 | 引导用户点击 Wi-Fi |

加数回调的关键两步是：

~~~c
tap_count += 10;  // 先改变数据
lv_label_set_text_fmt(count_label, "计数：%" PRIu32, tap_count);
~~~

清零回调同样要改变数据并刷新界面：

~~~c
tap_count = 0;
lv_label_set_text(count_label, "计数：0");
~~~

如果只把标签改成“计数：0”，而不把 tap_count 置零，下次加数会继续使用旧变量值。

绑定事件这句：

~~~c
lv_obj_add_event_cb(button, on_button_clicked, LV_EVENT_CLICKED, NULL);
~~~

四个参数依次是：哪个对象、执行哪个函数、响应哪类事件、附带什么自定义数据。最后的 NULL 表示本例没有附加数据。

### on_timer() 为什么换了写法

早先使用 lv_tick_get() / 1000，得到的是毫秒计数转换出的运行秒数。现在需要真实时间，使用：

~~~c
time_t now = time(NULL);
struct tm current_time;
localtime_r(&now, &current_time);

char text[32];
strftime(text, sizeof(text), "%Y-%m-%d %H:%M:%S", &current_time);
lv_label_set_text(label, text);
~~~

| 函数或字段 | 在这里做什么 |
| --- | --- |
| time(NULL) | 读取系统当前时间；这一步本身不会访问互联网 |
| struct tm | 把日期拆成年、月、日、时、分、秒等字段 |
| localtime_r() | 按已设置的时区转换时间，并写入调用者提供的结构体 |
| strftime() | 将结构体中的日期时间格式化成字符串 |
| %Y-%m-%d %H:%M:%S | 显示格式，例如“年-月-日 时:分:秒” |
| tm_year | 从 1900 开始计算的年数，因此比较年份时需要减 1900 |
| lv_timer_get_user_data() | 取回创建定时器时传入的 subtitle 标签 |
| wifi_setup_refresh() | 每秒同时刷新配网页的连接状态 |

当前代码把年份小于 2024 的时间视为尚不可用，先显示配网或校时提示，避免把 1970 年当作当前日期显示。

时区在网络模块启动时设置：

~~~c
setenv("TZ", "CST-8", 1);
tzset();
~~~

POSIX 时区字符串里的符号方向与直觉相反：这里 CST-8 表示 UTC+8。后续 localtime_r() 已经完成时区转换，不需要自己再加 8 小时。

### 为什么启动时加锁，点击回调中没有再次加锁

app_main() 和 LVGL 是不同任务。启动时创建对象，需要用 BSP 的显示锁保护。按钮、键盘和 LVGL 定时器的回调原本就在 LVGL 任务中执行，本例在这些回调内直接更新对象。

网络任务不直接调用 lv_label_set_text()，它只更新网络状态快照，再由 LVGL 定时器读取显示。这是本工程避免两个任务同时修改界面的方式。

## wifi_setup.c：屏幕配网页怎样工作

| 函数 | 做了什么 | 需要关注的地方 |
| --- | --- | --- |
| wifi_setup_open() | 创建列表页和输入页，默认显示列表并提交扫描 | 若设置层已打开，重复点击不会再创建第二份 |
| scan_clicked() / refresh_networks() | 提交扫描、读取结果快照、按修订号更新列表 | 每秒读取一次，但只有结果或扫描状态变化时才重建列表 |
| network_clicked() / show_form() | 将点选热点名称填入 SSID，并切到密码框 | 先复制选中记录，避免刷新快照使事件数据变化；切换热点时清空密码 |
| show_networks() / manual_clicked() | 返回热点列表，或打开手动输入 | 返回列表时解绑键盘并清空密码；手动入口用于隐藏热点 |
| make_button() | 创建按钮及其居中的文字 | 把重复的创建过程放进函数 |
| on_input() | 让屏幕键盘切换到当前点击或获得焦点的输入框 | 同一个键盘可以为不同输入框输入文字 |
| connect_clicked() | 读取输入、校验、提交连接请求 | 只提交请求，不等到连接完成才返回 |
| keyboard_event() | 处理键盘确认和退出事件 | SSID 确认后转向密码，密码确认后提交 |
| wifi_setup_refresh() | 读取扫描和连接状态并更新提示 | 输入错误只保留在输入页，不阻止扫描列表更新；后台任务不直接改标签 |
| close_setup() | 解绑键盘、清空密码输入框并延后删除设置层 | 清掉全局对象指针，避免定时器继续访问已删除对象 |

重要的 LVGL 用法：

~~~c
lv_textarea_set_one_line(s_ssid, true);
lv_textarea_set_password_mode(s_password, true);
lv_keyboard_set_textarea(s_keyboard, s_ssid);
~~~

第一句将名称输入框设为单行，第二句让密码以隐藏形式显示，第三句决定键盘当前向哪个输入框写入文字。

输入框设置了字符数上限，但最终校验仍然按字节数进行。因为一个中文汉字通常占多个 UTF-8 字节，“32 个字符”和“32 个字节”不一定相同。

s_local_error 的作用是保留本地输入错误提示。否则定时器每秒刷新一次，就会立即把“密码太短”等提示覆盖掉。用户继续编辑或重新提交后，才恢复正常状态显示。

close_setup() 使用 lv_obj_delete_async() 延后删除整个设置层，因为关闭动作本身也是这个界面的一次事件回调。删除前先将键盘与输入框解绑，再把保存的界面对象指针设为 NULL。

## 新增扫描：从打开页面到点选名称

~~~mermaid
flowchart TD
    A[打开 Wi-Fi 或点击 Rescan] --> B[wifi_clock_scan 提交 COMMAND_SCAN]
    B --> C[wifi_worker 标记 scan_pending]
    C --> D[scan_if_ready 等待当前连接尝试空闲]
    D --> E[启动 STA 并扫描附近热点]
    E --> F[取回 AP 记录并整理结果]
    F --> G[释放驱动列表和临时数组]
    G --> H[发布不含密码的扫描快照]
    H --> I[LVGL 定时器按 revision 更新列表]
    I --> J[点选热点并复制 SSID]
    J --> K[切到密码页 点击 Connect]
~~~

这里保留原来的连接状态机，增加了一份独立的 wifi_scan_info_t。扫描状态有 IDLE、WAITING、RUNNING、DONE、FAILED；连接是否已保存，仍由 wifi_clock_info_t 表达。扫描本身不会写 NVS，也不会修改已保存的名称和密码。

### 为什么网络任务等待扫描，屏幕仍能操作

~~~c
esp_wifi_scan_start(NULL, true);
~~~

第二个参数 true 表示等待扫描完成。调用位置是 wifi_worker()，不是 LVGL 的点击回调；所以等待期间屏幕和 ESP-IDF 事件循环仍在各自任务中执行。网络事件会先进入原来的消息队列，扫描返回后再处理。此处使用完成后的取回流程，没有新增 SCAN_DONE 界面回调。

Wi-Fi 驱动在正在连接时可能拒绝主动扫描，因此 scan_if_ready() 会等当前连接尝试成功或进入重试间隔，再开始搜索。已有连接时可以搜索，失败重连时则优先在下一次重试前完成待处理扫描。反复按 Rescan 不会堆积扫描：atomic_bool s_scan_requested 只允许一份排队或正在执行的请求。

结果处理先调用 esp_wifi_scan_get_ap_num()，最多为 64 条记录分配临时堆内存，再调用 esp_wifi_scan_get_ap_records() 取回按 RSSI 排列的记录。驱动为扫描分配了内存，必须取回或清理；代码在成功、空结果和失败路径都执行清理，并 free() 自己的临时数组。大数组没有放进网络任务的栈。

最终快照用原来的临界区保护，界面拿到完整副本。revision 只在扫描状态或结果变化时递增；refresh_networks() 发现修订号没有变化就直接返回，避免每秒重建按钮导致滚动位置跳动。

### wifi_networks.c：去重与信号排序

wifi_network_t 保存原始 SSID、RSSI、是否需要密码和当前连接代码是否支持该安全类型，不保存密码或路由器 BSSID。wifi_networks_add() 不依赖 ESP-IDF 或 LVGL，所以可以用本机 GCC 验证：

1. 忽略没有名称或没有字符串结束符的记录。
2. 同一名称、相同密码需求和相同支持状态只保留一项；后来的信号更强时替换。
3. 插入或替换后向前移动，保持 RSSI 从强到弱排列。
4. 最多保留 WIFI_NETWORK_LIMIT，即 20 项；列表已满时丢弃更弱的新记录。

SSID 按字节原样复制，32 字节名称和中文 UTF-8 名称不会因为显示而被修改。列表只在显示时把换行等控制字符替换为问号，连接仍使用原始名称。开放网络和需要密码的同名网络分开显示，避免把两类网络混为一个选项。

### 字体与文件清理

当前使用从思源黑体转换的本工程点阵字库，英文与常用符号沿用 Montserrat。旧 16 像素 CJK 字库已不再编入固件；三个新字体 C 文件都被 ui_fonts.c 引用并参与构建，字体授权随文件保留。具体覆盖范围和转换步骤见后面的“中文字体怎样加入”。

停用的 Wi-Fi 配置头文件和未被加载的 VS Code 示例模板已经删除；当前源码都在 main/CMakeLists.txt 中注册，两个 tests 文件都有 README 中可直接运行的测试命令。构建目录与 managed_components 是实际使用的生成文件和依赖，不属于停用源码，继续由构建工具管理且不提交仓库。

## wifi_credentials.c：输入校验与 C 字符串

配置结构体包含：

~~~c
typedef struct {
    char ssid[33];
    char password[65];
} wifi_credentials_t;
~~~

SSID 最长 32 字节，密码最长 64 字节；数组多出的一个位置用来保存 C 字符串结束符 \0。

wifi_credentials_error() 返回：

- NULL：输入通过校验。
- 一个文字指针：具体错误说明，界面可以直接显示。

例如：

~~~c
const char *error = wifi_credentials_error(ssid, password);
if (error != NULL) {
    lv_label_set_text(s_message, error);
    return;
}
~~~

bounded_length() 最多读取指定数量的字符。它的作用是检查上限时不无止境向后寻找结束符。后续只有输入通过校验，才会复制进固定大小的配置数组。

64 字节密码需要单独检查：它必须是十六进制 PSK，不能把任意 64 字节字符串都当作普通口令交给驱动。

## 中文字体怎样加入

### 三个实际使用的字号

| 字体文件 | 覆盖与精度 | 用在什么位置 |
| --- | --- | --- |
| s31_cn_16.c | ASCII 和当前全部中文提示/标点，4 bpp | 扫描状态、连接状态、输入错误、热点认证说明 |
| s31_cn_24.c | GB2312 的 6763 个常用汉字和当前界面补充字符，2 bpp | 热点名称、SSID 输入框、网络按钮、配网页标题和主界面提示 |
| s31_cn_32.c | 从 main.c 当前文字提取中文字符，4 bpp | 计数、加数和清零按钮；数字仍由 Montserrat 32 绘制 |

bpp 表示一个点阵像素使用多少位来保存灰度。24 像素字库字符多，使用 2 bpp 控制 Flash 占用；两个界面专用小字库使用 4 bpp。三个字库均为未压缩点阵，普通构建直接编译这些 C 文件，不需要下载原始 OTF 或安装转换工具。

fonts 下的 C 文件是自动生成的点阵数据，文本体积大，不等于固件占用相同大小；复习时重点阅读 ui_fonts.c 和 generate_fonts.py，无需逐行阅读点阵数组。

24 像素字库的 6763 个汉字来自 GB2312 汉字区；当前界面额外使用的中文标点也加入了字库。覆盖范围外的字符仍可能显示为方框。字库负责显示，英文字母/数字/符号键盘的输入方式沿用原界面。

### 为什么加了字库还要设置字体

UTF-8 字符串保存的是字符编码，字体提供对应编码的点阵；两者都正确，中文才会显示。只启用一个字体选项，不会自动给所有标签换字体。

app_main() 在显示锁内先调用 ui_fonts_init()，再调用 create_screen()。ui_fonts.c 创建 Montserrat 字体结构体副本，将 24/32 像素字体的 fallback 分别指向同字号中文字体。英文、数字和图标沿用原字体，中文由点阵字库补足；不会修改 LVGL 自带的只读对象。

主界面和配网页通过 ui_font_small()、ui_font_normal()、ui_font_large() 取得字体。配网页原先只给 SSID 设置的小字号备用字体已移除，现在中文标题、按钮、状态提示和热点名称都连接到统一字体接口。用户原先修改的 My first screen 英文标题保留，仍使用原来的 48 像素英文字体。

### 字体来源与重新生成

字库从 Adobe [Source Han Sans（思源黑体）官方仓库](https://github.com/adobe-fonts/source-han-sans)的 SourceHanSansSC-Normal.otf 转换，使用 [LVGL 官方 lv_font_conv](https://github.com/lvgl/lv_font_conv) 1.5.3。原始字体 SHA-256 为 `7e5b7e586262f5e187c3397935b3a0dfbe28e08a916be0f60ec7fee2b50abd6d`。字体授权和版权声明保存在 main/fonts/OFL-SourceHanSans.txt，随字库一并保留。

以后新增中文提示，先改 C 代码，再重新生成 16/32 像素界面字库并运行覆盖检查。安装有 Node.js/npm 的电脑，可把工具放在工程外的临时目录，避免干扰源码目录：

~~~powershell
npm install --prefix ../s31-font-tools lv_font_conv@1.5.3
python tools/generate_fonts.py --font "D:/fonts/SourceHanSansSC-Normal.otf" --converter "../s31-font-tools/node_modules/lv_font_conv/lv_font_conv.js"
python tests/font_coverage_test.py
~~~

示例中的 OTF 路径替换为实际下载位置。generate_fonts.py 会读取界面字符串、生成 GB2312 汉字集合，按三个字号转换，并移除自动生成头注释中的本机绝对路径。编译列表、字体使用、测试和授权都已列明，没有保留停用的旧字库文件。

font_coverage_test.py 不依赖转换工具或开发板：它直接解析生成 C 文件中的 cmaps、glyph_dsc 和 glyph_bitmap，检查字形是否存在、汉字是否有点阵、点阵索引是否越界，以及 24 像素字库是否保持在默认字形索引上限以内。

## wifi_clock.c：一次连接请求走过哪些函数

~~~mermaid
flowchart TD
    A[屏幕点击 Connect] --> B[校验名称和密码]
    B --> C[请求复制到队列]
    C --> D[网络后台任务]
    D --> E[连接热点并获取 IP]
    E --> F[保存配置到 NVS]
    E --> G[启动 SNTP 校时]
    G --> H[更新系统时间]
    H --> I[LVGL 定时器刷新标签]
    F --> J[下次开机读取并自动连接]
~~~

### 对外接口：先看 wifi_clock.h

| 接口 | 谁调用 | 返回或产生什么 |
| --- | --- | --- |
| wifi_clock_start() | app_main() 调用一次 | 完成初始化并创建网络后台任务，不等待连接完成 |
| wifi_clock_connect(ssid, password) | 配网页的 Connect 回调 | 校验并复制输入，向队列提交请求；ESP_OK 只代表提交成功 |
| wifi_clock_get_info(&info) | 主界面和配网页 | 获取连接状态、是否保存、SSID、错误码；不返回密码 |
| wifi_clock_scan() | 打开配网页和 Rescan 回调 | 去重正在排队/执行的扫描请求，提交 COMMAND_SCAN，不在界面任务等待扫描 |
| wifi_clock_get_scan(&scan) | 配网页刷新回调 | 复制扫描状态、修订号、结果数量、名称/信号/安全类型；没有密码 |

“请求提交成功”“Wi-Fi 连接成功”“保存成功”“网络校时成功”是不同阶段，界面应按真实阶段显示状态。

### 启动过程

wifi_clock_start() 依次处理：

1. 设置时区。
2. 初始化 NVS、网络接口和默认事件循环。
3. 创建 STA 网络接口。STA 表示开发板作为客户端连接路由器。
4. 创建消息队列。
5. 初始化 SNTP，但先不启动校时。
6. 初始化 Wi-Fi 驱动，选择 STA 模式，并注册网络事件回调。
7. 创建 wifi_worker() 后台任务。

后台任务启动后先调用 read_saved_network()。读到有效配置就自动连接；没有配置就进入 WIFI_CLOCK_SETUP_REQUIRED，等待屏幕输入。

### 为什么要用队列复制输入

wifi_clock_connect() 没有把输入框内部的文字指针直接交给后台任务，而是把名称和密码复制进 wifi_command_t，再将整条消息交给队列。

原因是界面可能被关闭、输入框也可能被删除。如果后台保留的是输入框内部地址，稍后再读取就可能访问失效内存。复制消息后，后台任务持有自己的数据副本。

同时，界面点击函数提交消息后就返回，连接等待和重试不会堵住触摸响应。

### 网络事件也交给同一个后台任务

on_wifi_event() 将 ESP-IDF 网络事件转成内部消息：

| ESP-IDF 事件 | 内部消息 | 后台做什么 |
| --- | --- | --- |
| WIFI_EVENT_STA_START | DRIVER_STARTED | 发起连接 |
| WIFI_EVENT_STA_STOP | DRIVER_STOPPED | 旧连接停完后，开始使用新配置 |
| WIFI_EVENT_STA_DISCONNECTED | DRIVER_DISCONNECTED | 决定重试或提示失败 |
| IP_EVENT_STA_GOT_IP | DRIVER_GOT_IP | 验证当前目标、保存配置、启动校时 |

界面提交的 COMMAND_CONNECT、COMMAND_SCAN 和这些驱动事件都在 wifi_worker() 中串行处理，避免多个任务同时修改驱动过程。STA_START 如果只为了扫描而启动驱动，不会自动发起连接，只有 attempt_active 为真才连接。

### 关键函数各负责什么

| 函数 | 职责 |
| --- | --- |
| request_connection() | 接收新的网络配置；若旧 Wi-Fi 正在运行，先停止并等停止事件 |
| start_candidate() | 把候选名称与密码交给驱动，启动 STA，并设置连接超时 |
| connect_now() | 发起一次实际连接请求 |
| fail_attempt() | 结束当前尝试；新配置提示失败，已验证网络安排后续重连 |
| handle_connected() | 检查当前连接确实对应目标 SSID，成功后保存并启动校时 |
| read_saved_network() | 读取并检查已保存记录的版本、长度和字符串格式 |
| save_network() | 写入一个包含名称和密码的记录，并提交 NVS |
| set_info() | 更新不含密码的连接状态快照 |
| scan_if_ready() | 在当前连接尝试结束后的空闲时段执行扫描，取回结果并释放驱动列表 |
| supported_auth() | 只允许开放和支持的 WPA2/WPA3 个人网络从列表点选；旧 WEP、企业认证等显示不支持 |

switching 标志用于处理换网时的中间阶段。停止旧 Wi-Fi 后，不能立刻把旧断线事件当成新网络失败，也不能把旧 IP 事件当成新网络连接成功。因此等待 DRIVER_STOPPED 后才应用新配置，收到 IP 时再检查实际连接的 SSID。

### controller_t 里的状态变量

| 字段 | 意义 |
| --- | --- |
| credentials | 当前正在尝试或使用的名称与密码 |
| running | STA 驱动是否已经启动 |
| switching | 是否正在等待旧 STA 停止，以便换网 |
| auto_reconnect | 配置来自已保存记录，或本次已经成功连接，可以持续重连 |
| save_pending | 新配置尚未成功保存 |
| attempt_active | 当前尝试是否仍应处理结果 |
| scan_pending | 有一次扫描请求等待连接空闲时执行 |
| retries | 新网络已经进行的重试次数 |
| deadline_us | 当前尝试的截止时间，单位微秒 |
| retry_at_us | 下次发起重连的时间，单位微秒 |

这些变量表达的是“连接过程走到哪一步”。如果只用一个 connected 布尔值，就无法同时表达“正在换网”“已连接但没保存”“输入的新密码错误”等情况。

### 界面看到的状态

| 状态 | 含义 |
| --- | --- |
| WIFI_CLOCK_STARTING | 网络模块正在初始化 |
| WIFI_CLOCK_SETUP_REQUIRED | 没有可用的已保存配置，等待屏幕输入 |
| WIFI_CLOCK_CONNECTING | 正在尝试连接或重连 |
| WIFI_CLOCK_CONNECTED | 已经获取 IP，配置已保存或来自已保存记录 |
| WIFI_CLOCK_CONNECT_FAILED | 新配置连接失败，可以修改后重试 |
| WIFI_CLOCK_SAVE_FAILED | 已连接，但写入 NVS 没成功 |

s_info 用临界区保护。后台更新它时不会同时被界面读到一半；界面取走的是一份完整快照。_Atomic 队列句柄则用于安全发布初始化后的队列地址。

## NVS：为什么驱动用 RAM，配置还能在重启后恢复

代码仍然调用：

~~~c
esp_wifi_set_storage(WIFI_STORAGE_RAM);
~~~

这句话只决定 Wi-Fi 驱动自己的配置保存方式。本工程选择自行管理持久保存：

~~~text
屏幕输入新配置
    ↓
先只交给 RAM 中的驱动配置
    ↓
认证成功并得到 IP
    ↓
save_network()
    ↓
nvs_set_blob() + nvs_commit()
~~~

如果让驱动在设置配置时直接保存，新密码还没验证就可能替换旧记录。现在由应用决定保存时机，因此输错的新密码不会覆盖上一次成功记录。

NVS 读写过程：

~~~text
nvs_open("s31_wifi", ...)
    ↓ 打开本工程的命名空间
nvs_get_blob(..., "station", ...) 或 nvs_set_blob(...)
    ↓ 读取或写入一个完整记录
nvs_commit(...)
    ↓ 写操作提交
nvs_close(...)
~~~

名称、密码和版本号作为一个 blob 保存，而不是分别保存两个互不关联的字段。read_saved_network() 会检查版本、大小及字符串结束符，避免把不符合当前格式的内容当作配置。

这里“连接成功”的标准是取得 IP 地址，不要求 NTP 服务器已经回复。所以 Connected and saved 之后，时钟仍可能短暂显示等待校时，这是正常的两个阶段。

## 三种时间单位，复习时不要混淆

| 位置 | 单位 | 本工程用途 |
| --- | --- | --- |
| lv_timer_create(..., 1000, ...) | 毫秒 | 每秒刷新一次界面 |
| time(NULL) | 秒 | 读取系统日期时间 |
| esp_timer_get_time() | 微秒 | 计算 30 秒连接超时和约 5 秒重试间隔 |

wifi_worker() 平时每次等待队列最多约 200 毫秒，随后检查截止时间。执行扫描时，该网络任务会等待驱动返回几秒，网络事件暂存在队列中；LVGL 和 ESP-IDF 事件任务仍独立运行，界面回调本身不等待扫描或连接。

## 二次学习建议：每次只做一个小实验

| 实验 | 改或操作什么 | 预期结果 |
| --- | --- | --- |
| 1. 数据与文字 | 只把按钮文字改成 +100，不改 tap_count += 10 | 实际仍加 10，说明文字不决定数据 |
| 2. 更新周期 | 将 LVGL 定时器周期暂时改为 2000 | 屏幕约两秒刷新一次，系统时间本身没有停止 |
| 3. 本地校验 | 输入 7 字节密码后点 Connect | 立即提示格式问题，不必等 Wi-Fi 超时 |
| 4. 正确保存 | 输入可用网络，看到 Connected and saved 后重启 | 自动连接该网络 |
| 5. 错误配置保护 | 成功保存后，尝试一个错误密码，失败后重启 | 仍使用之前成功保存的配置 |
| 6. 异步界面 | 连接过程中回到主界面，点击加数和清零 | 按钮仍可响应，网络在后台继续处理 |
| 7. 校时与连接 | 观察 Connected and saved 与时间出现的先后 | 理解取得 IP 与收到校时响应是不同阶段 |

实验后把不想保留的改动恢复，再进行下一项。先掌握“对象、事件、状态、定时器”，再逐渐阅读“任务、队列、持久存储”。


## 摄像头实时预览：本次全部改动

这一课先实现本机屏幕实时预览。主界面仍保留计数、网络配置和时钟，右上角新增“摄像头”。进入后显示 OV3660 的实时画面；“重试”重新开始一次采集，“返回”停止采集并回到主界面。预览不依赖 Wi-Fi，当前没有拍照保存、录像或浏览器视频流。

### 文件改动对照

| 文件 | 改了什么、为什么 |
| --- | --- |
| `main/main.c` | 引入 `camera_preview.h`，新增“摄像头”按钮并绑定 `camera_preview_open()`；底部提示同时介绍网络和摄像头。英文标题文字保留，纵坐标由 55 改成 64，给新增入口留出间距。 |
| `main/camera_preview.c / .h` | 新增预览页面、重试与返回事件、一个后台采集任务、V4L2 初始化/帧队列、显示双缓冲、超时与错误提示以及退出清理。头文件只公开页面入口。 |
| `main/camera_frame.c / .h` | 新增不依赖 IDF/LVGL 的纯 C 函数：检查尺寸、处理行间填充、最近邻缩小、180 度旋转及 RGB565 大端转小端。独立出来便于阅读和在电脑上验证。 |
| `main/CMakeLists.txt` | 将两个新 `.c` 文件注册到实际编译列表。 |
| `main/idf_component.yml` | 显式加入 `espressif/esp_video: "2.2.0"`，让应用能直接使用 V4L2 视频头文件；更新工程描述。BSP 已间接依赖同一版本，这次没有换摄像头驱动版本。 |
| `dependencies.lock` | 组件管理器更新直接依赖列表和清单哈希；注册表地址格式自动规范化。实际依赖版本保持不变。 |
| `sdkconfig.defaults` | 开启 OV3660、DVP 自动检测、640x480 RGB565 BE 10 FPS 模式，选为默认输出；关闭原默认 YUV 模式；保留 DVP 视频设备，并关闭 BSP 的遇错立即中止选项。详细配置见下一节。 |
| 本机 `sdkconfig` | 同步上述有效选项，使当前电脑实际构建使用摄像头配置。它是自动生成文件，仍不提交 Git。 |
| `tools/generate_fonts.py` | 字符提取范围加入 `camera_preview.c`，以后修改摄像头提示也能生成对应点阵。 |
| `main/fonts/s31_cn_16.c / s31_cn_32.c` | 重新生成小号提示和主界面字体，补齐摄像头用字。24 像素常用汉字库已覆盖新文字，重新生成后的内容相同，因此没有重复修改。 |
| `tests/font_coverage_test.py` | 将摄像头页面纳入实际字形检查。当前界面共 198 个非 ASCII 字符。 |
| `tests/camera_frame_test.c` | 新增颜色字节序、旋转、非整数缩小比例、带填充行、空指针、尺寸/长度溢出及完整预览大小的边界检查。 |
| `README.md / WIFI_CLOCK.md` | 更新功能入口、烧录与升级配置方法，记录本次全部修改、原理、验证方法和实机待确认项。 |

没有添加停用模板、备用图片或另一套未调用驱动。新增的源文件全部被实际调用，测试和字体脚本用于复习与验证。临时工具、测试可执行文件与编译日志放在工程外；构建缓存和固件继续由 `.gitignore` 排除。

### 摄像头配置与旧工程升级

`sdkconfig.defaults` 中增加：

~~~ini
CONFIG_CAMERA_OV3660=y
CONFIG_CAMERA_OV3660_AUTO_DETECT_DVP_INTERFACE_SENSOR=y
CONFIG_CAMERA_OV3660_DVP_RGB565_BE_640X480_10FPS=y
CONFIG_CAMERA_OV3660_DVP_DEFAULT_FMT_RGB565_BE_640X480_10FPS=y
CONFIG_CAMERA_OV3660_DVP_YUV422_YUYV_640X480_10FPS=n
CONFIG_ESP_VIDEO_ENABLE_DVP_VIDEO_DEVICE=y
CONFIG_BSP_ERROR_CHECK=n
~~~

前四项启用传感器、自动检测和选定的 RGB565 模式。第五项关闭这次不用的 YUV 格式，避免默认输出与图像转换函数不一致。第六项启用 DVP 视频设备。最后一项让 BSP 的错误检查返回错误码；应用可显示初始化失败提示，屏幕启动失败时也已有检查。

**本电脑的实际 `sdkconfig` 已同步。** 新克隆且没有 `sdkconfig` 时，会自动应用 defaults。旧克隆已经存在 `sdkconfig` 时，单改 defaults 通常不会替换原值：可用 `idf.py menuconfig`，按 `/` 搜索上面的配置名，依次启用 OV3660、对应格式与默认格式，并关闭 BSP_ERROR_CHECK；保存后重新编译。生成后的配置中，布尔关闭项显示为 `# CONFIG_... is not set` 是正常的。

这些选项可以在 `build/config/sdkconfig.h` 中核对。预期 `CONFIG_CAMERA_OV3660_DVP_IF_FORMAT_INDEX_DEFAULT` 为 `2`。应用也会通过 `VIDIOC_G_FMT` 检查实际格式，若不是 640x480 RGB565 BE，会提示检查配置，而不会把 YUV 数据当作 RGB 显示。

摄像头 GPIO、20 MHz XCLK 和 SCCB/I2C 总线由官方 BSP 提供，不在应用里重复写一份引脚表。使用 `BSP_CAMERA_DEVICE` 打开当前 DVP 设备，使用 `BSP_CAMERA_ROTATION` 处理板上默认的 180 度安装方向。

### 从点击按钮到一帧画面

~~~text
主界面点击“摄像头”
    -> camera_preview_open() 创建 LVGL 预览页
    -> start_requested_session() 发布新的会话编号，通知后台任务
    -> camera_worker() 串行执行 capture_session()
    -> 首次调用 bsp_camera_start() 初始化传感器和视频设备
    -> open(BSP_CAMERA_DEVICE) 打开当前采集会话
    -> VIDIOC_G_FMT 检查尺寸、像素格式与行跨度
    -> VIDIOC_S_DQBUF_TIMEOUT 设置 250 ms 等帧超时
    -> VIDIOC_REQBUFS 申请两个采集缓冲区
    -> VIDIOC_QUERYBUF + mmap 获取驱动缓冲区地址
    -> VIDIOC_QBUF 把空缓冲区交给驱动
    -> VIDIOC_STREAMON 开始采集
    -> 循环：DQBUF 取已完成帧 -> 转换到显示后缓冲 -> QBUF 归还采集帧
    -> publish_frame() 加显示锁后切换 LVGL 图像源
    -> 返回或重试：旧会话 STREAMOFF -> close -> 释放显示缓冲区
~~~

`QBUF` 表示“这个缓冲区可再次用于采集”，`DQBUF` 表示“取走一帧供应用读取”。图像处理完必须归还采集缓冲区，即使该帧带错误标记也要归还，否则驱动会逐渐没有可用缓冲区。错误帧不会送到屏幕。驱动拥有 MMAP 采集内存，正常停止并 `close()` 后由驱动释放，应用不能对这些指针调用 `free()`。

这一版使用 esp_video 2.2.0 的超时 ioctl。它的 VFS 不使用 `O_NONBLOCK`；单纯给 `open()` 加该标志不能保证等待帧及时退出。空队列超时在该版本映射成 `ESP_FAIL`，VFS 的 errno 为 `EPERM`，所以代码将它与 `ETIMEDOUT/EAGAIN` 一起作为暂时没有帧处理。连续约 3 秒没有收到有效帧就停止当前预览并显示检查排线提示。250 ms 是单次取帧等待上限，完整退出还包含驱动停止和清理时间。

### 为什么要转换和准备两份显示缓冲

RGB565 用 16 位表示一个像素：红色 5 位、绿色 6 位、蓝色 5 位。例如纯红像素是 `0xF800`，摄像头大端字节依次为 `F8 00`；本机 LVGL RGB565 小端数据应为 `00 F8`。若直接把原始字节送去显示，可能出现错误颜色。

`camera_frame_to_rgb565()` 在一次遍历中完成：

1. 从 640x480 最近邻取样为 480x360，保持 4:3 比例，不拉伸为全屏。
2. 根据 BSP 的方向要求，从源图像另一端读取，得到 180 度旋转结果。
3. 每个像素交换两个字节，输出 LVGL 使用的 RGB565 数据。

单个预览缓冲为 `480 * 360 * 2 = 345600` 字节。两个采集帧通常共 `640 * 480 * 2 * 2 = 1228800` 字节，两个显示帧共 `691200` 字节，合计约 1.83 MiB；此外还有 LCD 帧缓冲、驱动队列和任务栈。显示帧明确从 PSRAM 申请，申请失败会显示提示。

后台只写“后缓冲”，准备完整后才拿 `bsp_display_lock()` 切换图像源；LVGL 绘制也使用同一个锁，所以不会一边绘制、一边改写同一份像素。切换成功后两份显示缓冲交换角色；如果暂时拿不到锁，就复用未显示的后缓冲，跳过这次显示更新。描述符地址重复使用时，先用 `lv_image_cache_drop()` 清掉对应旧缓存。

`lv_image_dsc_t` 告诉 LVGL 像素地址、宽高、颜色格式、行跨度和数据长度。**仅有原始像素指针还不足以描述一幅图像。** 清理前必须在显示锁内隐藏图像、解除图像源引用并清除缓存，随后才能释放像素和结束描述符的生命周期。

### 重试、快速返回和失败处理

每次打开或重试都会生成新的会话编号，`_Atomic` 发布给后台；编号 `0` 表示关闭。后台用同一任务处理全部采集，只在旧会话清理后启动最新会话。因此连续点击重试、快速返回再进入，不会同时启动两个采集过程，也不会让旧任务把图像更新到新页面。

界面回调仅创建对象、切换状态和发通知，不在触摸线程内等帧或等待摄像头初始化。网络与时钟继续由原任务运行。摄像头任务第一次进入页面才创建，退出后保留任务等待通知；再次打开会复用它。

当前 BSP 1.0.1 的 `bsp_camera_start()` 没有配套 camera stop API，也没有把 XCLK 句柄交给应用。因此整个启动周期只调用一次，包括失败的情况；初始化成功后，退出页面停止的是采集流、关闭本次设备句柄并释放帧缓冲，底层注册和 XCLK 保留到重启。初始化失败后需断电检查排线并重启，“重试”只重试已初始化设备的采集过程。

若极少见的 `STREAMOFF` 停止失败，不能确认 DMA 已结束。这时故意保留驱动采集内存和设备句柄，阻止再次采集并提示重启，避免释放仍可能被硬件访问的内存；界面显示缓冲仍可解除引用后释放。正常退出则回收本次全部帧缓冲。

### 实机验证步骤与学习练习

1. 断电确认 OV3660 摄像头与排线已接好，再上电、编译并烧录。接线调整必须在断电时进行。
2. 点击“摄像头”，等待实时画面；串口应出现 `Camera initialization: ESP_OK` 和 `Preview started`。
3. 对准有红、绿、蓝物品和文字的场景，检查颜色、上下方向与左右方向。当前模式设定为 10 FPS，实际速度需结合显示负载实测。
4. 点击“返回”，检查主界面可加数、清零，时钟继续更新；串口应出现 `Preview ended; display buffers released`。
5. 多次进入、返回和重试，检查画面可恢复、没有崩溃或逐次内存不足。快速返回后再进入也应只运行最新请求。
6. 若显示初始化失败，断电检查排线方向、连接及 OV3660 型号，再重启；若格式不符，先核对上一节的实际配置。

学习时先读 `camera_preview_open()` 的页面创建，再顺着上面的 V4L2 流程读 `capture_session()`，最后读 `camera_frame_to_rgb565()`。可以先只把预览宽高改成 320x240，重新编译并观察画面与内存变化；两项同时调整才能继续保持 4:3。改变采集分辨率则还需要选择驱动实际支持的传感器模式，不能只改两个宏。

本机转换测试命令见 README。它验证数学处理和内存边界，不能代替真实摄像头采集、帧率、排线、供电和触摸响应测试。

参考：[官方开发板指南](https://docs.espressif.com/projects/esp-dev-kits/zh_CN/latest/esp32s31/esp32-s31-korvo-1/user_guide.html)、[ESP Video 编程说明](https://docs.espressif.com/projects/esp-video-components/zh_CN/latest/esp32p4/ESP_Video/index.html)。后者页面目标为 P4，用于理解通用接口；本工程的 S31 引脚和支持选项以固定版本 BSP 1.0.1、esp_video 2.2.0、esp_cam_sensor 源码为准，不照搬 P4 的 MIPI 配置。

## 已验证与待实机确认

- 当前摄像头版已在本电脑的 ESP-IDF master（v6.2.0）上编译通过并生成固件；固件大小为 `0x264ab0`（2509488）字节，3 MiB 应用分区还有 `0x9b550`（636240）字节，约 20% 空间。
- 本机 GCC 输入边界测试通过：空值、32/33 字节 SSID、UTF-8 超长、开放网络、7/8/63 字节密码、64 位十六进制 PSK、65 字节密码等。
- 本机 GCC 扫描结果测试通过：重复热点保留较强项、开放与加密同名分开、中文和 32 字节名称原样保留、空/未结束名称被忽略、20 项上限及满列表替换后排序。
- 字体覆盖检查通过：包含摄像头提示的 198 个非 ASCII 字符及 6763 个 GB2312 汉字都有有效字形；检查直接读取生成的 C 字体映射、字形描述和点阵范围。
- 摄像头转换测试通过：RGB565 红绿蓝颜色、180 度旋转、非整数缩放、行跨度、无效参数与长度溢出拒绝、480x360 输出前后保护字节检查。
- 使用同一套 Kconfig 定义从空配置生成一次测试配置，确认仅应用 `sdkconfig.defaults` 也会开启 OV3660、RGB565 640x480 默认模式、DVP 设备并关闭 BSP_ERROR_CHECK；生成结果放在工程外，不修改当前实际配置。
- 构建仍有本机 IDF master 及依赖的 CMake/Kconfig 提示（包含 SDK 私有 include 依赖、Windows 对象文件路径长度和 FATFS 布尔默认值），未阻止生成固件；新摄像头 C 文件编译没有警告。
- 尚未替用户烧录，也未使用真实 Wi-Fi 密码验证设备联网；实机仍需测试自动扫描、列表滚动、点选名称、中文显示、触摸键盘、成功保存、重启恢复及错误密码处理。摄像头尚需按上一节验证真实画面、颜色方向、帧率、反复进入/退出与错误恢复。

## 当前范围

配网页已支持自动扫描和点选名称；最多读取信号最强的 64 条 AP 记录，整理后显示最多 20 项。隐藏热点可手动输入。界面与 GB2312 常用中文名称可以显示，覆盖范围外的字符可能显示方框；屏幕键盘提供英文字母、数字和符号。当前支持常见 WPA2/WPA3 个人网络和开放网络，不包含企业网络或需网页认证的热点。

摄像头当前为 OV3660 本机预览，传感器模式为 640x480 RGB565 BE / 10 FPS，显示为 480x360 RGB565 LE；未实现拍照文件保存、录像与网络视频流。实际采集与显示仍待开发板确认。

## GitHub 同步改了什么

为建立 ESP-S31 仓库，额外整理了以下文件；这些改动不改变板上功能：

- `.gitignore`：增加忽略本机 `.vscode/settings.json`、固件和测试可执行文件。原有 `build`、`managed_components`、生成的 `sdkconfig`、旧私有 `main/wifi_config.h` 继续排除。
- `.gitattributes`：源码和文档在 Git 中统一采用 LF 换行，方便跨 Windows/Linux 复现。
- `.vscode/settings.json`：本机实际设置保留且不上传；原先提供的 settings.example.json 模板在本次清理中删除，新电脑直接由 ESP-IDF 扩展管理设置。
- `README.md`：补充仓库链接、克隆命令、在新电脑打开工程的方法和无需开发板即可运行的输入校验测试命令；说明克隆目录默认叫 `ESP-S31`，本机目录仍叫 `s31_display`。
- 上传范围为源码、配置模板、分区表、依赖锁文件、测试和学习文档；不上传构建缓存、固件或旧密码文件。

本机 IDF 环境和串口仍通过 ESP-IDF 扩展选择；复制到其他电脑后，需要按该电脑的实际安装情况重新选择。

2026-10-19 将上述学习工程同步到 [HYL886/ESP-S31](https://github.com/HYL886/ESP-S31) 的 `main` 分支。仓库当前为公开仓库，上传的是不含真实 Wi-Fi 凭据的源码快照。以后继续修改功能时，应同时补充本文的改动原因和验证方法，再提交到 Git。

## 官方资料

- [S31 Wi-Fi API](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s31/api-reference/network/esp_wifi.html)
- [S31 扫描与连接场景](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s31/api-guides/wifi-driver/station-scenarios.html)
- [S31 NVS 持久存储](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s31/api-reference/storage/nvs_flash.html)
- [系统时间与 SNTP 校时](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s31/api-reference/system/system_time.html)
