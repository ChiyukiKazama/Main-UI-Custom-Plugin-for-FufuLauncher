# 7.2 构建适配

## 支持的程序

| Profile | 进程名 | PE TimeDateStamp | SizeOfImage | 程序 SHA256 |
| --- | --- | --- | --- | --- |
| CN72 | `YuanShen.exe` | `0x6AD886FB` | `0x1B65F000` | `8b014f64b5dc12fbc476e0c362b053abfdd7a1378cd282e4ad81057621dbd6b5` |
| OS72 | `GenshinImpact.exe` | `0x6AD8992C` | `0x1B690000` | `b3001fc110b63e0e0ce24a77386e71d0eefaffaae244ea0c72645a659cb5ea6e` |

国际服样本版本为 `OSRELWin7.2.0_R49086080_S49291486_D49291486`。
SHA256 用于维护时识别原始样本；运行时匹配进程名、PE 指纹与全部入口字节，不在启动时散列整个游戏程序。

国服入口保留 `9e90088` 的数据。国际服入口来自对应程序的只读离线分析，以及运行时所属类的方法表核对。游戏程序、元数据、内存快照和个人配置不进入仓库或发布包。

## 数据与初始化

- `Native.h`：原有国服入口地址与校验字节。
- `NativeProfile.h`：40 个入口的语义索引、29 个主页面字段的语义索引、两服的构建数据，以及两个交互键位实例字段。
- `ElementPolicy`：只记录字段的语义身份，不决定某个游戏构建的偏移。
- `Engine`：首先选择 Profile，然后校验全部入口，最后安装挂钩。所有序列化字段读取来自同一个已选 Profile。

进程名采用 ASCII 大小写不敏感匹配；指纹精确匹配。不按文件名猜版本，不在运行时扫描相似函数，不将未适配程序回退到另一服的数据。

两服当前 `MonoInLevelMainPage` 的序列化布局相同，分别保留在各自 Profile 内。国际服已实例化的字段通过主页面组件与实际 GameObject 路径核对，包括地图、按钮组、各独立右上按钮、聊天、队伍、技能、底部状态、延迟与交互区域。交互键位字段同时与 `MonoInteePanel` 的两个刷新函数核对，为 `0x50`、`0x68`。其他插件中同一版本的其他类可能采用不同布局，不能将其偏移迁移到主页面。

国际服托管入口所属类核对：

| 入口 | 所属类 | RVA |
| --- | --- | --- |
| WillRender | Canvas，实际渲染回调 | `0x1A320650` |
| MainAwake | MonoInLevelMainPage | `0x12601150` |
| GraphicEnable / GraphicLightActivate | Graphic | `0x1A342F90` / `0x1A343080` |
| EffectEnable | MonoUIEffect | `0xABF77F0` |
| PrefabInstance | MonoPrefabPlugin | `0xCF56180` |
| QuestEnable | MonoQuestHintText | `0xABCEC00` |
| QuestTraceTick | MonoQuestTrace | `0x8779D20` |
| InteeDialogAdd / InteePickupAdd | MonoInteePanel | `0xCF21000` / `0xCF21380` |
| InteeKeyRefresh / InteeSwitchKeyRefresh | MonoInteePanel | `0xCF20800` / `0xCF1FE50` |
| MaskSetParamsDirty / MaskRebuild | SmoothMask | `0x1A3679E0` / `0x1A369E10` |
| RectMaskPerformClipping | RectMask2D | `0x1A34B3A0` |

原生 Unity icall 继续使用实际目标地址，保持原有输出缓冲区 ABI。两个国际服 Canvas 包装入口的前导指令相似；这里使用已经实际捕获到调用的渲染入口，而不是只按相似字节选择。

## 自动验证

```powershell
MSBuild.exe tests/NativeProfileTests.vcxproj /p:Configuration=Release /p:Platform=x64
./bin/x64/Tests/NativeProfileTests.exe
./bin/x64/Tests/NativeProfileTests.exe 'C:/Games/Genshin Impact/GenshinImpact.exe'
```

本地使用 VS 2022 时，可明确追加 `/p:PlatformToolset=v143`；默认构建和 CI 仍使用 v145。

不带参数的测试验证两服选择、进程名大小写、错误文件名／指纹拒绝、入口完整性和全部 41 条 HUD 策略绑定。CI 在打包前构建并执行该测试。

带参数时，只读解析本机程序的 PE section 表，将 RVA 转换为文件偏移，核对对应 Profile 的全部入口字节。国际服样本结果为 **40/40 通过**。不使用 `LoadLibrary` 加载游戏，不启动游戏，也不安装挂钩。

## 国际服游戏内验证

以下为上述国际服构建、芙芙启动器及技能 CD 插件 1.0.3 同时启用时的验证结果：

| 操作 | 结果 |
| --- | --- |
| 隐藏左上按钮组，保留小地图与任务追踪 | 通过 |
| 隐藏活动、祈愿、纪行、千星入口，保留手册／背包／角色 | 通过 |
| 剩余右上按钮自动右对齐 | 通过 |
| 隐藏延迟与聊天入口，保留队伍、技能及血条 | 通过 |
| Home 恢复原始界面，再次按下恢复自定义 | 通过 |
| End 导出主界面节点 | 通过 |
| 派蒙菜单打开并返回后保持自定义 | 通过 |
| 隐藏按钮时使用 Tab 快捷轮盘 | 通过 |
| 热重载 Scale=0.85、四边 Margin=30 | 通过，小地图、按钮、队伍、技能与底部状态同步更新 |
| 恢复 Scale=1、四边 Margin=0 | 通过，恢复原始布局 |
| 释放战技后切换角色，后台队伍行显示冷却数字与圆环 | 通过，与 UI 自定义同时工作 |

测试后恢复原有用户配置。原有显隐方式、缓存策略、配置格式、默认值和快捷键保持不变。

## 后续版本维护

新增构建时，同时提供进程名、PE 指纹、40 个入口及其完整校验字节、主页面字段布局和交互字段布局。先运行 Profile 测试与本机程序校验，再验证显隐、布局、快捷键、菜单生命周期和技能 CD 共存。不能只放行新进程名或只替换入口地址而沿用未核对的字段。
