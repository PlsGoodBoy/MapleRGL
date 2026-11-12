# 开发说明

## 结构

| 路径 | 内容 |
| --- | --- |
| `src/game.*` | 与渲染分离的战斗模拟 |
| `src/boss_combat.*`、`src/enemy_roles.cpp` | 首领专属招式、节点、阶段与普通敌人协同 |
| `src/adventure_catalogue.inc`、`src/adventure_content.cpp` | 技能定义和多源融合入口 |
| `src/fusion_transaction.cpp`、`src/skill_adventure.cpp` | 自动融合事务和多拍技能行为 |
| `src/run_equipment.*`、`src/resonance.cpp` | 装备、共鸣及贡献归属 |
| `src/skill_visual*`、`src/skill_sfx.hpp` | 表现事件、主题渲染和限流合成音效 |
| `src/hud_ui.cpp`、`src/camp_ui.cpp` | 构筑、升级、营地和设置 |
| `src/progression.*` | 存档、成长和结算 |
| `assets/manifest.json` | 素材路径、帧时序和原始锚点 |
| `docs/assets/` | 素材来源及复用记录 |
| `tests/`、`tools/` | 定向检查、资源导出与发布工具 |

## 构建与发布

使用Visual Studio 2022、Windows SDK和CMake构建。`tools/build_release.ps1` 使用 `build-release/`，开发候选位于 `bin-release/`，最终便携包固定在 `release/`。

代码或素材更新后运行发布脚本，更新EXE、assets、运行库、README与VERSION。发布说明源文件为 `tools/release_README.txt`。发布目录不包含存档、开发截图和测试日志，不向Git提交生成的EXE。

发布到 GitHub 时，先更新 `release/`，再运行 `python tools/package_release.py`，生成 `dist/` 中的 Windows x64 ZIP 与 `SHA256SUMS.txt`。Release 标签指向这次发布的源码提交，附上运行方法、更新说明和这两个下载附件。

GitHub 只保留最新 Release 和附件：先上传并核对新包，再发布新版本并清理旧 Release。保留正常源码提交历史，二进制不进入 Git。应用图标原图与多尺寸 ICO 位于 `resources/`，更新方式见其中的说明。

## 定向检查

按改动范围选择检查，不需要每次重复所有模拟。主要的当前玩法检查为：

```powershell
cmake --build build-release --config Release --target adventure_tests
.\build-release\Release\adventure_tests.exe
```

该目标也接受 `content`、`gear`、`save` 子集。运行环境与程序均使用独立诊断存档，正式用户存档保持独立。

程序提供 `--smoke-test`、`--preview-adventure-ui N`、`--preview-build-ui N`、`--preview-upgrades`、`--vfx-scene N` 等开发入口。配合 `--save-dir <目录>`、`--profile-output <JSON>` 和 `--frames <数量>` 使用，产物放入被忽略的 `artifacts/`。脚本构筑和后台采样不能代替自然难度试玩或前台帧率测量。

首领相关检查为 `enemy_profiles_tests` 和 `enemy_cooperation_tests`；存档检查为 `progression_tests`、`chapter_save_tests`、`save_migration`。只运行本次改动关联的目标。

`--boss-fight N --review-build N` 提供无无敌的隔离交战，`--playthrough --seed N --speed 2` 使用普通初始生命和合法自动输入运行经典模式并结算。两者都不是人工试玩。`--complete-run` 保留为带无敌的时间流程样本，不用于胜率或难度结论。`review_combat OUTPUT.json` 是三个固定种子的无界面样本，不能用于FPS结论。

`--review-ui-flow` 用630帧固定流程向ImGui输入队列注入数字键、Esc和真实控件坐标的鼠标事件，验证选卡、自动融合、替换取消/确认与重置；检查点和输入写入诊断JSON，失败或流程未完成返回非零退出码。这不覆盖操作系统焦点和真实设备输入。

程序将构建版本、内容版本、Git标识与构建时间写入设置页及诊断JSON。错误日志位于所选存档目录的 `logs/` 下，只在资源、配置或保存发生实际错误时写入。

## 资源与表现

资源工具按功能命名：`extract_combat_assets.py`、`extract_equipment_icons.py`、`extract_regions.py`、`extract_scene_props.py`、`extract_skill_icons.py`、`extract_enemy_appearances.py`、`extract_expansion_assets.py`、`extract_adventure_assets.py`、`extract_visual_effects.py`。提取需要本地原始NX和Python/Pillow，普通构建不需要NX。

素材清单保留源路径、锚点、帧时长和复用关系。战场召唤和弹体采用真实纹理；机械、蝙蝠、卡牌和部分融合使用明确标注的同主题资源。详见[战场素材与动画](BATTLE_ART.md)。

`extract_boss_timing.py` 读取原始攻击释放节点，生成首领动作时序与来源表；不修改原始NX。首领三招、核心机制和对应素材见[首领图鉴](BOSSES.md)与[素材映射](assets/boss_combat_sources.json)。

技能表现事件上限128，命中反馈上限40，近邻同源反馈合并；密集场景削减装饰火花、幻影和尾迹，保留主要攻击轮廓与危险提示。技能音效最多12声部，并按施放、命中、收尾分别限流。
