# OpenCPN Android Docker 编译包

这个包提供了编译OpenCPN Android原生库文件(.so)的完整Docker环境和文档。

**注意**: 此版本专注于生成.so库文件，不包含APK构建和打包。生成的库文件可在其他Android项目中使用。

## 📦 包含文件

### 📚 文档
- **`DOCKER_ENVIRONMENT.md`** - Docker环境详细说明和配置
- **`SO_COMPILATION.md`** - .so库文件编译指南和命令
- **`WXLIB_GUIDE.md`** - wxlib预编译库详细指南 (⭐ 重要)
- **`README.md`** - 本说明文件
- **`requirements.txt`** - 系统要求和依赖说明

### 🐳 Docker文件
- **`Dockerfile.android`** - Android构建环境的Docker镜像配置
- **`docker-compose.android.yml`** - Docker Compose配置文件

### 🔧 构建脚本
- **`build-android-libs.sh`** - 库文件编译脚本（推荐使用）

## 🚀 快速开始

### 前置要求
1. **Docker Desktop** 或 **Docker Engine** 已安装并运行
2. 至少 **8GB** 可用磁盘空间
3. 稳定的网络连接
4. **OpenCPN源码** 包含 `wxlib` 预编译库

### 使用步骤

#### 1. 检查环境
```bash
# 检查Docker环境
docker --version

# 检查wxlib完整性 (重要!)
find wxlib/lib -name "libwx_*.a" | wc -l    # 应该显示 22
find wxlib/lib -name "libQt5*.so" | wc -l   # 应该 > 0
```

#### 2. 执行编译
```bash
# 进入OpenCPN项目根目录
cd /path/to/OpenCPN-master_new

# 执行库文件编译脚本
./build-android-libs.sh
```

#### 3. 验证结果
```bash
# 检查库文件目录
ls -la android_libs/armeabi-v7a/

# 检查构建清单
cat android_libs/armeabi-v7a/MANIFEST.txt
```

## 📋 生成结果

### 库文件目录结构
```
android_libs/
└── armeabi-v7a/
    ├── libgorp.so              # 主程序库 (204MB)
    ├── libchartdldr_pi.so      # 海图加载插件 (3.8MB)
    ├── libdashboard_pi.so      # 仪表板插件 (840KB)
    ├── libgrib_pi.so           # 天气插件 (1.2MB)
    ├── libwmm_pi.so            # 地磁模型插件 (4.5MB)
    ├── libUNARR.so             # 压缩文件支持 (388KB)
    └── MANIFEST.txt            # 构建清单
```

### 库文件信息
- **总大小**: 约214MB
- **架构**: ARMv7 (armeabi-v7a)
- **最低Android版本**: API 21 (Android 5.0)
- **目标Android版本**: API 33 (Android 13)


## 🔥 wxlib预编译库 (关键依赖)

**wxlib是OpenCPN Android构建中最重要的依赖库**，包含wxQt框架的预编译版本。

**为什么wxlib至关重要？**
- ✅ **避免2-4小时的源码编译**：wxQt从源码编译极其复杂
- ✅ **确保版本兼容**：预编译库已经过充分测试
- ✅ **提供完整GUI框架**：包含所有必要的wxWidgets和Qt5组件
- ❌ **无法替换**：OpenCPN Android版本必须使用这个特定的wxlib版本

**验证wxlib完整性**：
```bash
# 检查关键文件数量
find wxlib/lib -name "libwx_*.a" | wc -l    # 应该显示 22
find wxlib/lib -name "libQt5*.so" | wc -l   # 应该显示 5+

# 详细验证请参考: WXLIB_GUIDE.md
```

## 📊 系统要求

### 硬件要求
- **内存**: 推荐8GB+ RAM
- **存储**: 至少8GB可用空间
- **网络**: 稳定的互联网连接

### 软件要求
- **操作系统**: macOS, Linux, Windows (支持Docker)
- **Docker版本**: 20.10+ 或 Docker Desktop 4.0+

### 时间要求
- **首次构建**: 20-30分钟 (包含Docker镜像构建)
- **后续构建**: 10-15分钟
- **Docker镜像大小**: 约3.14GB

## 🚨 常见问题

### 1. wxlib缺失或损坏
```bash
# 检查wxlib完整性
find wxlib/lib -name "libwx_*.a" | wc -l
# 如果不是22，参考 WXLIB_GUIDE.md 修复
```

### 2. Docker权限问题
```bash
# Linux用户执行
sudo usermod -aG docker $USER
# 重新登录或重启
```

### 3. 磁盘空间不足
```bash
# 清理Docker缓存
docker system prune -a
# 删除构建文件
rm -rf build_android/
```

### 4. 网络连接问题
```bash
# 重启Docker服务
sudo systemctl restart docker  # Linux
# 或重启Docker Desktop (macOS/Windows)
```

## 📞 获取帮助

如果遇到问题：
1. 查看 `DOCKER_ENVIRONMENT.md` 了解Docker环境
2. 查看 `SO_COMPILATION.md` 了解编译流程
3. 参考 `WXLIB_GUIDE.md` 解决wxlib相关问题
4. 检查构建日志中的错误信息

## 📞 技术支持

- **OpenCPN版本**: 5.13.0
- **Android API**: 21-33
- **架构**: ARMv7 (armeabi-v7a)
- **文档版本**: 1.2 (仅库文件编译)
- **更新日期**: 2025-10-20

---

**祝你构建顺利！🚢 库文件已准备就绪，可以在你的Android项目中使用了！**