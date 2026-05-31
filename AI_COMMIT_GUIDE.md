# AI 代码提交指南

## 核心规则：每次修改代码都必须提交

本仓库要求所有 AI 工具在修改代码时遵循以下规则，确保每次变更都有迹可循，方便回退。

---

## 提交规范

### 何时提交

- **每次代码修改完成后立即提交**，不要批量提交多个不相关的改动
- 一个 commit 只包含一个逻辑变更（单文件或多文件均可，但必须是同一目的）
- 提交前检查变更内容，确保没有误改其他文件

### Commit Message 格式

```
<type>: <简短描述>

[可选的详细说明]
```

**type 类型：**
- `feat` — 新功能
- `fix` — 修复 bug
- `refactor` — 重构代码（不改变功能）
- `style` — 代码格式调整（空格、缩进等）
- `docs` — 文档变更
- `test` — 测试相关
- `chore` — 构建、依赖、配置等杂项

**示例：**
```
feat: 添加用户登录功能

- 新增 LoginPage 组件
- 添加 token 持久化逻辑
```

### 提交流程

```bash
# 1. 查看变更
git status

# 2. 查看具体改动
git diff

# 3. 添加文件
git add <file1> <file2>

# 4. 提交
git commit -m "<type>: <描述>"
```

### 禁止事项

- **禁止**一次性提交所有文件（避免 `git add .` 或 `git add -A`，应指定具体文件）
- **禁止**提交包含敏感信息的文件（密钥、密码、token、.env 等）
- **禁止**跳过 Git hooks（如 `--no-verify`），除非用户明确要求
- **禁止**在 commit message 中使用 emoji

### 回退方法

如需回退某次更改：

```bash
# 查看历史
git log --oneline

# 回退到某个 commit（保留工作区改动）
git reset <commit-hash>

# 回退到某个 commit（丢弃工作区改动）
git reset --hard <commit-hash>

# 撤销某次 commit 的改动（生成新的反向 commit）
git revert <commit-hash>
```

---

> **记住：每一次更改都是一次记录。让 Git 历史成为项目最可靠的变更日志。**
