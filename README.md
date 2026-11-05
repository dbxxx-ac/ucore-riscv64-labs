# Lab3 提交文件

小组成员：母焯行（2513960）、罗熙媛（2514039）、谢北宸（2513513）。

- `report/report.md`：实验报告。
- `report/prompt.md`：两个模块的提示词设计整理版，以及自动关机与源码整理的实际补充提示词。
- `kern/`、`libs/`、`tools/`、`Makefile`：2026 年 11 月 5 日 Ubuntu 工作目录传回的源码和构建工具。
- `validation/`：该版本在 Ubuntu 上的构建、评分和默认 QEMU 运行记录。

在配置好 RISC-V 交叉工具链和兼容的 QEMU 后，从本目录运行：

```bash
make clean
make
make grade
make qemu
```

已保存的验证结果：清理后构建退出码 0；评分 100/100；默认 QEMU 输出 10 次 `100 ticks` 后自动关机，退出码 0。`SHUTDOWN_AFTER=10` 是本组选择的输出次数。

压缩包未包含 `obj/`、`bin/` 或 Git 历史。解压后的本目录即构建根目录。将文件放入确定的 Lab3 提交仓库后，再完成 Git 提交与推送；本包本身不表示已推送到远程。
