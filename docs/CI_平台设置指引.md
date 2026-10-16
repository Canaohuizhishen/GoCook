# GoCook CI — 平台设置指引（G 类：只能网页操作）

> 仓库的 CI 工作流（`.github/workflows/`）已全部入库并跑通；但有几项设置**只能在 GitHub 网页 / 外部服务上完成**，AI 无法代做。本文是逐条操作指引（约 10 分钟）。
> 背景与分批说明见仓库外的任务书 `提案/GoCook 全套 CI 搭建提案.md`（§G 平台设置）。

徽章一览（README 顶部）：CI / CodeQL / Lint / gitleaks 来自各工作流自带状态；覆盖率来自 Codecov。

---

## 一、必备：Rulesets —— 让 CI 的「红叉」真正拦住合并

**目的**：把 CI / CodeQL 设为**必需状态检查**，未通过就不能并入 `dev`。

1. 打开 `https://github.com/Canaohuizhishen/GoCook/settings/rules`（仓库 → **Settings → Rules → Rulesets**）。
2. **New ruleset → New branch ruleset**。
3. **Name** 填 `dev-protect`（任意）。
4. **Enforcement status** 选 `Active`。
5. **Target branches** → Add target → **Include default branch**（本仓库默认分支即 `dev`）。
6. 勾选 **Require status checks to pass**，点 **Add checks** 搜索并选中（名字 = 工作流里的 job `name:`）：
   - `CI / 构建 + 测试（ubuntu-24.04）`
   - `CodeQL / CodeQL 分析（C/C++，手动构建）`
   - （可选）`Lint / clang-format 格式检查`、`CI / 覆盖率（gcov + gcovr）`
   - ⚠️ 检查项必须**先成功跑过至少一次**才会出现在搜索框里（本仓库已满足）。
7. （可选）勾 **Require a pull request before merging** 以禁止直推。
8. **Create**。

> 之后向 `dev` 直推 / PR 合并时，上述检查未绿会被拦下。临时放行：Rulesets 页把该条改为 **Disabled**。

## 二、必备：Security —— 打开依赖与密钥告警

打开 `https://github.com/Canaohuizhishen/GoCook/settings/security_analysis`（**Settings → Security → Security analysis**），确认：

- **Dependabot alerts**：Enabled（依赖漏洞告警）
- **Dependabot security updates**：Enabled（自动提修复 PR；与 `dependabot.yml` 的版本升级互补）
- **Secret scanning**：Enabled（平台级密钥扫描，覆盖面比 CI 内的 gitleaks 更广）
- **Push protection**：建议 Enabled（推送含密钥时直接拦截）

> `gitleaks.yml` 是 CI 内的密钥扫描；这里的是 GitHub 平台自带，二者不冲突，**建议都开**。

## 三、覆盖率徽章：接入 Codecov

README 的覆盖率徽章来自 **Codecov** —— `https://codecov.io/gh/Canaohuizhishen/GoCook`。

**CI 侧已就绪**：`coverage` job 生成 `build/coverage.xml`，并用 **OIDC 免令牌**方式上传（`codecov/codecov-action@v5` + `use_oidc: true`；job 已加 `id-token: write` 权限）。

**你只需在 Codecov 侧启用该仓库**：

1. 打开 `https://codecov.io/`，用 **GitHub 账号登录**。
2. 在 **Not yet setup** 列表里找到 `Canaohuizhishen/GoCook`，点 **Configure / Setup repo**（按提示安装 **Codecov GitHub App** 并授予该仓库访问）。
3. 完成后徽章即会显示；首次上传后稍等 Codecov 消化覆盖率数据。

**若 OIDC 上传被拒**（少数情形，如组织未启用 OIDC 集成）→ 回退用上传令牌：

1. Codecov 仓库页 → **Settings → General → Repository Upload Token**，复制 token。
2. GitHub 仓库 → **Settings → Secrets and variables → Actions → New repository secret**，名 `CODECOV_TOKEN`。
3. 把 `.github/workflows/ci.yml` 里 Codecov 步骤的 `use_oidc: true` 行删掉，并给该步骤加：
   ```yaml
         env:
           CODECOV_TOKEN: ${{ secrets.CODECOV_TOKEN }}
   ```

> **令牌红线**：本项目的 git 推送一律走你自己的凭据，AI 不接触任何推送令牌。这里的 **Codecov token 是「覆盖率上传令牌」而非推送令牌**，只用于上传覆盖率，不参与任何 git 操作。

## 四、发布相关（可选）

- **ghcr 镜像可见性**：`release.yml` 把镜像推到 `ghcr.io/canaohuizhishen/gocook-server`。包默认继承仓库可见性（公开仓库 → 公开包）。如需调整：`https://github.com/users/Canaohuizhishen/packages/container/gocook-server/settings`。
- **打第一个 Release**：`release-server` job 只在 `v*` tag 上运行（这就是 dev push 时「打包 server 二进制」被 Skip 的原因）。要发版：
  ```bash
  git tag v1.0.0
  git push origin v1.0.0
  ```

## 五、Actions 权限（一般无需改）

**Settings → Actions → General → Workflow permissions**：保持默认即可。本仓库各工作流已在 YAML 内显式声明最小 `permissions`（如 `contents: read`；Codecov 用 `id-token: write`；发布用 `contents: write` / `packages: write`）。

---

_最后核对：2026-10-16。工作流或徽章如与本文不一致，以 `.github/workflows/` 与 `README.md` 为准。_
