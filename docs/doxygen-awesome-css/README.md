# doxygen-awesome-css（vendored）

Doxygen 文档主题 [doxygen-awesome-css](https://github.com/jothepro/doxygen-awesome-css)，
供仓库根 `Doxyfile` 的 `HTML_EXTRA_STYLESHEET` 引用（Sidebar-Only 布局）。

- 来源：<https://github.com/jothepro/doxygen-awesome-css>
- 版本：v2.5.0（官方声明兼容 doxygen 1.9.1–1.9.4 与 1.9.6–1.18.0；本仓库在 1.18.0 静态二进制上生成验证）
- 许可证：MIT（见同目录 `LICENSE`）
- 使用要求（见仓库根 Doxyfile）：`GENERATE_TREEVIEW = YES`、`FULL_SIDEBAR = NO`、`DISABLE_INDEX = NO`、`HTML_COLORSTYLE = LIGHT`

## 更新方法

在本目录执行（替换为新版本 tag 后重新生成文档即可）：

```bash
curl -fL -o doxygen-awesome.css              https://raw.githubusercontent.com/jothepro/doxygen-awesome-css/v2.5.0/doxygen-awesome.css
curl -fL -o doxygen-awesome-sidebar-only.css https://raw.githubusercontent.com/jothepro/doxygen-awesome-css/v2.5.0/doxygen-awesome-sidebar-only.css
curl -fL -o LICENSE                          https://raw.githubusercontent.com/jothepro/doxygen-awesome-css/v2.5.0/LICENSE
```

> 注：不放在 `third_party/`——该目录在 `run.sh` 的「服务端源码新于镜像即重建」检测范围内，
> 放入纯文档资产会误触发服务端镜像重建。
