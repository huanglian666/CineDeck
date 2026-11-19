# CineDeck

跨平台桌面端媒体库管理应用。Rust 侧基于 GPUI 框架构建界面与业务逻辑，底层计算由 C 模块（`c_core/`）以静态库形式提供，通过 FFI 链接。

> **当前状态**：项目处于早期开发阶段，C 计算模块已完成，Rust 侧 FFI 接入尚未开始。Rust / GPUI 相关功能均未实现，本文只描述已落地的部分。

## 整体架构

```
┌─────────────────────────────────────────────┐
│  Rust 侧（规划中，尚未实现）                  │
│  GPUI 桌面界面 · 媒体库业务逻辑              │
│  文件读取 · 流式切块状态机 · FFI 调用封装     │
└──────────────────┬──────────────────────────┘
                   │ FFI（静态链接）
┌──────────────────▼──────────────────────────┐
│  c_core/（已完成）                           │
│  无状态底层计算封装，CMake 构建为静态库       │
│  ├─ cinedeck_md4_digest                     │
│  └─ cinedeck_ed2k_hash_from_chunk_digests   │
└──────────────────┬──────────────────────────┘
                   │
            rhash（vcpkg 管理）
```

分工思路：

- **C 计算模块（`c_core/`）**：聚焦无状态的底层计算与算法封装，不持有文件句柄、不做 I/O。
- **Rust 侧**：负责文件读取、流式切块、上层业务与界面逻辑，把每一块的摘要结果交给 C 模块聚合。

## c_core 计算接口

头文件位于 `c_core/include/ed2k_calc.h`，实现位于 `c_core/src/ed2k_calc.c`，基于 [rhash](https://github.com/rhash/RHash) 实现。

### `int cinedeck_md4_digest(const uint8_t *data, size_t len, uint8_t out_digest[16])`

计算单段字节缓冲区 `[0..len)` 的 16 字节 MD4 摘要。

- 支持 `len == 0` 且 `data == NULL` 的空输入场景（输出空串 MD4）。
- 返回码：`0` 成功；`-1` `out_digest` 为空；`-2` `data` 为空但 `len > 0`；`-3` rhash 初始化失败；`-4` MD4 计算失败；`-5` 结果写入失败。

### `int cinedeck_ed2k_hash_from_chunk_digests(const uint8_t *chunk_digests_concat, size_t chunk_count, uint8_t out_digest[16])`

按 ED2K 分块（标准块大小 **9,728,000 字节**）各块的 16 字节 MD4 连续拼接结果，计算最终 ED2K 哈希。

- `chunk_digests_concat` 为连续存放的 `chunk_count * 16` 字节摘要缓冲区。
- 单块（`chunk_count == 1`）直接拷贝；多块将全部摘要拼接后再计算一次 MD4。
- 约定 `chunk_count > 0`：空文件由上层视作单块处理，不向该聚合函数传入 0 块。
- 返回码：`0` 成功；`-1` 参数不合法；`-2` rhash 初始化失败；`-3` 摘要拼接计算失败；`-4` 结果写入失败。

## 目录结构

```
cinedeck/
├── c_core/                  # C 计算模块
│   ├── include/ed2k_calc.h  # 对外 API 声明
│   ├── src/ed2k_calc.c      # 基于 rhash 的实现
│   ├── test_ed2k.c          # 单元测试 + 真实文件流式分块计算示例
│   └── CMakeLists.txt       # 静态库构建脚本
├── vcpkg.json               # vcpkg 依赖清单（rhash）
├── vcpkg_installed/         # vcpkg 本地安装产物（gitignore）
└── .github/workflows/       # CI：Windows / macOS 静态库构建
```

## 依赖与构建

### 依赖

- CMake ≥ 3.20，C11 编译器
- [vcpkg](https://github.com/microsoft/vcpkg)，依赖清单见根目录 `vcpkg.json`（当前仅 `rhash`）
  - macOS：`arm64-osx` triplet
  - Windows：`x64-windows-static` triplet（静态 CRT）

### 本地构建

```bash
# 1. 安装 vcpkg 依赖（已在仓库根目录执行过则跳过）
vcpkg install --triplet arm64-osx        # macOS Apple Silicon
# vcpkg install --triplet x64-windows-static   # Windows x64

# 2. 配置并编译静态库
cmake -S c_core -B c_core/build -DCMAKE_BUILD_TYPE=Release

# 3. 编译并运行单元测试（可选）
cmake -S c_core -B c_core/build -DCMAKE_BUILD_TYPE=Release -DCINEDECK_BUILD_TESTS=ON
cmake --build c_core/build
./c_core/build/test_ed2k                     # 基础单元测试
./c_core/build/test_ed2k /path/to/file.mkv   # 真实文件流式分块 ED2K 计算示例
```

说明：

- 未显式传 `CMAKE_TOOLCHAIN_FILE` 时，CMake 会自动回退到仓库根目录的 `vcpkg_installed/arm64-osx` 定位 rhash；传入 vcpkg toolchain 时则按指定的 triplet 定位（Windows CI 即采用这种方式）。
- 构建产物为静态库：macOS 为 `libed2k_calc.a`，Windows 为 `ed2k_calc.lib`。

### CI 构建产物下载

GitHub Actions 工作流 [`.github/workflows/c_core-build.yml`](.github/workflows/c_core-build.yml) 会自动在 Windows 和 macOS 两个平台编译静态库并运行单元测试：

- 触发条件：`main` / `dev` 分支推送、PR 涉及 `c_core/**` 或 `vcpkg.json`，或在 Actions 页面手动触发（`workflow_dispatch`）。
- 每次运行结束后，可在该次运行页面底部的 **Artifacts** 区下载：
  - `c_core-arm64-osx`（macOS，`libed2k_calc.a`）
  - `c_core-x64-windows-static`（Windows，`ed2k_calc.lib`）
- 产物内含 `include/`（`ed2k_calc.h` 与 rhash 头文件）和 `lib/`（静态库本体及 rhash 静态库），可直接供后续 Rust 侧 FFI 链接使用。

## 开发进度

### 已完成

- [x] `cinedeck_md4_digest`：单段 MD4 摘要计算，含参数校验与各返回路径的资源释放
- [x] `cinedeck_ed2k_hash_from_chunk_digests`：ED2K 分块摘要聚合（单块拷贝 / 多块拼接再算 MD4）
- [x] 基础单元测试与本地真实文件流式分块（9,728,000 字节/块）计算验证
- [x] CMake 静态库构建配置，导出符号核验正常
- [x] GitHub Actions CI：Windows / macOS 静态库自动构建与产物下载

### 进行中 / 后续计划

- [ ] Rust 侧接入：FFI 声明、构建链接（build.rs 链接 `libed2k_calc` 与 `librhash`）
- [ ] Rust 侧流式文件切块状态机与 ED2K 哈希计算全链路
- [ ] 基于 GPUI 的桌面界面与媒体库管理能力
