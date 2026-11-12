# 应用图标

`maplergl.png` 是已确认的原始 RGBA 设计稿，由内置图像生成工具制作；红橙枫叶、深青绿底与金色细边，外围透明。它是本项目的应用图标设计，不是从原游戏提取的技能图标。

`maplergl.ico` 包含 16、24、32、48、64、128、256 像素七种尺寸，由原图导出，保留透明度。资源编译器将它嵌入 EXE，窗口大小图标使用相同资源。运行不依赖旁置 ICO 文件。

更新源图后运行 `python tools/build_app_icon.py`（需要 Pillow），再运行 `powershell -NoProfile -ExecutionPolicy Bypass -File tools/build_release.ps1`。

生成提示词保留在 [icon-prompt.txt](icon-prompt.txt)。
