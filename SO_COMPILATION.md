# OpenCPN Android .so库文件编译指南

## 📋 .so库文件概述

OpenCPN Android版本编译生成以下原生库文件：

**目标架构**: ARMv7 (armeabi-v7a)
**总大小**: 约214MB
**输出目录**: `./android_libs/armeabi-v7a/`

### 🎯 生成的主要库文件

| 文件名 | 大小 | 作用 | 依赖关系 |
|--------|------|------|----------|
| **libgorp.so** | 204MB | OpenCPN核心程序库 | wxWidgets + Qt5 + OpenGL ES2 |
| **libchartdldr_pi.so** | 3.8MB | 海图加载插件 | → libUNARR.so |
| **libdashboard_pi.so** | 840KB | 仪表板插件 | 独立 |
| **libgrib_pi.so** | 1.2MB | 天气插件 | 独立 |
| **libwmm_pi.so** | 4.5MB | 地磁模型插件 | 独立 |
| **libUNARR.so** | 388KB | 压缩文件支持 | 被chartdldr插件使用 |

### 🔥 关键依赖：wxlib预编译库

**wxlib是编译成功的决定性因素**：
- 包含wxWidgets 3.1 (22个预编译静态库)
- 包含Qt5.15 (Android动态库)
- 避免了2-4小时的源码编译时间
- 版本兼容性已验证

## 🚀 完整编译流程

### 前置条件检查
```bash
# 1. 检查Docker环境
docker --version

# 2. 检查wxlib目录 (关键!)
if [ ! -d "wxlib" ]; then
    echo "❌ wxlib目录不存在，编译将失败！"
    exit 1
fi

# 3. 检查wxlib完整性
WXLIB_COUNT=$(find wxlib/lib -name "libwx_*.a" | wc -l)
if [ $WXLIB_COUNT -eq 22 ]; then
    echo "✅ wxWidgets库文件完整: 22个"
else
    echo "❌ wxWidgets库文件不完整: $WXLIB_COUNT"
    exit 1
fi

# 4. 检查磁盘空间
AVAILABLE_SPACE=$(df . | tail -1 | awk '{print $4}')
if [ $AVAILABLE_SPACE -lt 8388608 ]; then  # 8GB in KB
    echo "❌ 磁盘空间不足，至少需要8GB"
    exit 1
fi
```

### 方法1: 一键编译脚本 (推荐)
```bash
# 执行库文件编译脚本
./build-android-libs.sh
```

**脚本执行过程**：
1. 🐳 构建Docker镜像 (10-15分钟)
2. 🔨 编译C++库 (10-15分钟)
3. 📦 整理库文件 (1分钟)
4. ✅ 验证构建结果

### 方法2: 分步手动编译

#### 步骤1: 构建Docker镜像
```bash
docker build --platform=linux/amd64 -f Dockerfile.android -t opencpn-android-builder:latest .
```

#### 步骤2: 编译C++库文件
```bash
docker run --rm -v "$(pwd):/src:Z" opencpn-android-builder:latest bash -c "
    cd /src
    rm -rf build_android
    mkdir -p build_android
    cd build_android

    # CMake配置 (关键参数)
    cmake -DOCPN_TARGET_TUPLE:STRING='Android-armhf;16;armhf' \\
          -DCMAKE_TOOLCHAIN_FILE=../buildandroid/build_android.cmake \\
          -Dtool_base=/opt/android-sdk/ndk/25.1.8937393/toolchains/llvm/prebuilt/linux-x86_64 \\
          -DCMAKE_PREFIX_PATH=/src/wxlib \\
          ..

    # 编译 (使用所有CPU核心)
    make -j\$(nproc)
"
```

**CMake参数说明**：
- `OCPN_TARGET_TUPLE='Android-armhf;16;armhf'`: 目标为ARMv7架构
- `CMAKE_TOOLCHAIN_FILE`: Android NDK工具链配置
- `CMAKE_PREFIX_PATH=/src/wxlib`: **关键参数**，指向预编译库目录

#### 步骤3: 整理库文件
```bash
docker run --rm -v "$(pwd):/src:Z" opencpn-android-builder:latest bash -c "
    cd /src/build_android

    # 创建输出目录
    mkdir -p android_libs/armeabi-v7a

    # 复制主程序库
    if [ -f 'libgorp.so' ]; then
        cp libgorp.so android_libs/armeabi-v7a/
        echo '✅ libgorp.so 已复制'
    else
        echo '❌ libgorp.so 未找到'
        exit 1
    fi

    # 复制插件库
    PLUGIN_COUNT=0
    for plugin_dir in plugins/*/; do
        if [ -d \"\$plugin_dir\" ]; then
            for so_file in \"\$plugin_dir\"/*.so; do
                if [ -f \"\$so_file\" ]; then
                    cp \"\$so_file\" android_libs/armeabi-v7a/
                    echo \"✅ \$(basename \"\$so_file\") 已复制\"
                    ((PLUGIN_COUNT++))
                fi
            done
        fi
    done
    echo \"✅ 总共复制了 \$PLUGIN_COUNT 个插件库\"

    # 创建构建清单
    cat > android_libs/armeabi-v7a/MANIFEST.txt << 'EOF'
OpenCPN Android Libraries Manifest
===================================

Build Information:
- Target Architecture: ARMv7 (armeabi-v7a)
- OpenCPN Version: 5.13.0
- Build Date: $(date)
- wxWidgets: 3.1 (precompiled)
- Qt5: 5.15 (Android)
- NDK: 25.1.8937393

Libraries:
EOF

    # 添加库文件信息到清单
    echo 'Main Library:' >> android_libs/armeabi-v7a/MANIFEST.txt
    for so_file in android_libs/armeabi-v7a/libgorp.so; do
        SIZE=\$(stat -c%s \"\$so_file\")
        echo \"  \$(basename \"\$so_file\"): \$SIZE bytes\" >> android_libs/armeabi-v7a/MANIFEST.txt
    done

    echo 'Plugin Libraries:' >> android_libs/armeabi-v7a/MANIFEST.txt
    for so_file in android_libs/armeabi-v7a/lib*.so; do
        if [ -f \"\$so_file\" ] && [[ \"\$so_file\" != *\"libgorp.so\" ]]; then
            SIZE=\$(stat -c%s \"\$so_file\")
            echo \"  \$(basename \"\$so_file\"): \$SIZE bytes\" >> android_libs/armeabi-v7a/MANIFEST.txt
        fi
    done

    echo '✅ 构建清单已创建'
"

# 复制到项目根目录
cp -r build_android/android_libs .
```

## 📁 生成的目录结构

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

### 文件详细说明

#### libgorp.so - 主程序库
- **大小**: 204MB
- **作用**: 包含完整的OpenCPN导航功能
- **依赖**: wxWidgets 3.1 + Qt5.15 + OpenGL ES2
- **功能**:
  - 海图渲染和显示
  - GPS数据处理
  - 导航计算
  - 用户界面
  - 插件系统管理

#### 插件库文件
- **libchartdldr_pi.so**: 海图加载器，支持BSB raster等格式
- **libdashboard_pi.so**: 仪表板，显示速度、航向、深度等
- **libgrib_pi.so**: 天气显示，处理GRIB天气数据
- **libwmm_pi.so**: 地磁模型，提供磁偏角计算
- **libUNARR.so**: 压缩文件解压支持

## ✅ 编译结果验证

### 快速验证脚本
```bash
#!/bin/bash
echo "=== OpenCPN Android库文件验证 ==="

# 1. 检查输出目录
if [ -d "android_libs/armeabi-v7a" ]; then
    echo "✅ 库文件目录存在"
    echo "  目录大小: $(du -sh android_libs | awk '{print $1}')"
else
    echo "❌ 库文件目录不存在"
    exit 1
fi

# 2. 检查主程序库
if [ -f "android_libs/armeabi-v7a/libgorp.so" ]; then
    SIZE=$(du -h android_libs/armeabi-v7a/libgorp.so | cut -f1)
    echo "✅ 主程序库: libgorp.so ($SIZE)"
else
    echo "❌ 主程序库缺失"
    exit 1
fi

# 3. 检查插件库
PLUGIN_COUNT=$(ls android_libs/armeabi-v7a/lib*.so | grep -v libgorp.so | wc -l)
echo "✅ 插件库数量: $PLUGIN_COUNT"

# 4. 检查总大小
TOTAL_SIZE=$(du -sh android_libs | cut -f1)
echo "✅ 总大小: $TOTAL_SIZE"

# 5. 检查清单文件
if [ -f "android_libs/armeabi-v7a/MANIFEST.txt" ]; then
    echo "✅ 构建清单存在"
    echo "构建信息:"
    cat android_libs/armeabi-v7a/MANIFEST.txt | head -10
else
    echo "❌ 构建清单缺失"
fi

# 6. 检查架构信息
if file android_libs/armeabi-v7a/libgorp.so | grep -q "ARM"; then
    echo "✅ 架构: ARMv7"
else
    echo "❌ 架构异常"
fi

echo ""
echo "=== 库文件列表 ==="
ls -lh android_libs/armeabi-v7a/*.so | awk '{print "  " $9 " (" $5 ")"}'

echo ""
echo "=== 验证完成 ==="
```

### 手动验证命令
```bash
# 检查文件完整性
ls -la android_libs/armeabi-v7a/

# 检查文件大小
du -sh android_libs/armeabi-v7a/*.so

# 检查架构信息
file android_libs/armeabi-v7a/*.so

# 检查构建清单
cat android_libs/armeabi-v7a/MANIFEST.txt
```

## 🚨 常见编译问题解决

### 1. wxlib缺失或损坏
```bash
# 检查wxlib目录
ls -la wxlib/

# 检查wxWidgets库文件数量
find wxlib/lib -name "libwx_*.a" | wc -l
# 应该显示 22

# 检查Qt5库文件
find wxlib/lib -name "libQt5*.so" | wc -l
# 应该 > 0
```

### 2. CMake配置失败
```bash
# 重新配置CMake
docker run --rm -v "$(pwd):/src:Z" opencpn-android-builder:latest bash -c "
    cd /src/build_android
    rm -rf CMakeCache.txt CMakeFiles/
    cmake -DOCPN_TARGET_TUPLE:STRING='Android-armhf;16;armhf' \\
          -DCMAKE_TOOLCHAIN_FILE=../buildandroid/build_android.cmake \\
          -Dtool_base=/opt/android-sdk/ndk/25.1.8937393/toolchains/llvm/prebuilt/linux-x86_64 \\
          -DCMAKE_PREFIX_PATH=/src/wxlib \\
          ..
"
```

### 3. 编译链接错误
```bash
# 检查库文件架构
docker run --rm -v "$(pwd):/src:Z" opencpn-android-builder:latest bash -c "
    file /src/wxlib/lib/libwx_baseu-3.1-armv7a-linux-androideabi28.a
    # 应该显示: ... ARM
"

# 重新编译
docker run --rm -v "$(pwd):/src:Z" opencpn-android-builder:latest bash -c "
    cd /src/build_android
    make clean
    make -j\$(nproc)
"
```

### 4. 磁盘空间不足
```bash
# 清理Docker缓存
docker system prune -a

# 清理构建文件
rm -rf build_android/

# 重新编译
./build-android-libs.sh
```

## 📊 编译时间参考

| 环境 | 首次构建 | 后续构建 | 说明 |
|------|----------|----------|------|
| **标准环境** | 20-30分钟 | 10-15分钟 | 包含Docker镜像构建 |
| **仅编译** | 10-15分钟 | 5-10分钟 | 重用Docker镜像 |
| **SSD硬盘** | 减少20% | 减少20% | 显著提升速度 |
| **高速网络** | 减少10% | 无影响 | 仅首次构建 |

## 🎯 成功标准

**✅ 编译成功的标志**：
1. **输出目录存在**: `android_libs/armeabi-v7a/`
2. **主程序库**: `libgorp.so` (~204MB)
3. **插件库完整**: 5个插件库文件
4. **构建清单**: `MANIFEST.txt`存在
5. **总大小**: 约214MB
6. **架构正确**: ARMv7 (32位)

**❌ 编译失败的标志**：
1. 输出目录缺失或为空
2. libgorp.so不存在或大小异常
3. 插件库数量不等于5
4. 架构不匹配 (非ARMv7)
5. 清单文件缺失

---

**编译完成后，.so库文件已准备就绪，可以在其他Android项目中使用！**