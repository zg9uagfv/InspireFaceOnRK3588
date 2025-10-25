# InspireFaceOnRK3588 相机人脸识别系统

本项目演示了如何使用 InspireFace SDK 和 OpenCV 进行实时人脸检测、跟踪和识别。它支持从摄像头实时捕获人脸图像，提取人脸特征，并与数据库中存储的人脸进行比对识别。

## 功能特性

- **实时人脸检测与跟踪**：从摄像头实时捕获视频流并检测人脸
- **3D人脸角度分析**：仅对正脸（正面）进行识别，过滤侧脸
- **人脸质量评估**：自动检测并过滤模糊或低质量的人脸图像
- **眼镜反光检测**：识别眼镜反光情况，避免因反光导致的识别错误
- **人脸特征提取**：提取高维人脸特征向量用于识别
- **数据库比对**：与本地数据库中存储的人脸特征进行高效比对
- **可视化界面**：在视频流中显示检测框、匹配结果和相似度
- **自动图像保存**：识别成功后自动保存人脸图像到本地
- **持久化存储**：使用SQLite数据库持久化存储人脸特征
- **批量导入**：支持从图像目录批量导入人脸特征
- **自动ID管理**：自动分配人脸ID，无需手动指定

## 先决条件

- OpenCV (4.5+)
- InspireFace SDK 库
- CMake (3.10+)
- 支持 C++14 的 C++ 编译器

## 项目结构

```
.
├── camera_face_recognizer.cpp     # 实时人脸识别主程序
├── add_face_to_database.cpp       # 人脸特征导入工具
├── CMakeLists.txt                 # CMake 构建配置
├── run_demo.sh                    # 一键演示脚本
├── test_face_recognition.sh       # 人脸识别测试脚本
├── include/                       # InspireFace 和 InspireCV 头文件
├── lib/                           # InspireFace 预编译库
├── model/                         # 模型文件目录
├── database/                      # 人脸特征数据库目录（运行时创建）
└── pic/                           # 识别成功的人脸图像目录（运行时创建）
```

## 构建项目

```bash
mkdir build
cd build
cmake ..
make
```

## 使用方法

### 1. 实时人脸识别

```bash
./camera_face_recognizer <模型目录路径> [摄像头索引]
```

参数说明：
- `模型目录路径`：包含 InspireFace 模型文件的目录路径
- `摄像头索引`：可选参数，指定摄像头设备（0 表示默认摄像头，默认值为 0）

示例：
```bash
# 使用默认摄像头和 model 目录中的模型
./camera_face_recognizer ../model

# 使用第二个摄像头和 Gundam_RK3588 目录中的模型
./camera_face_recognizer ../model/Gundam_RK3588 1
```

按 'q' 键退出应用程序。

### 2. 添加人脸到数据库

#### 2.1 从图像目录批量添加（推荐）

```bash
./add_face_to_database <模型目录路径> <图像目录路径>
```

示例：
```bash
# 从 face_images 目录批量添加所有人脸
./add_face_to_database ../model ./face_images
```

#### 2.2 从单个图像文件添加（兼容旧版）

```bash
./add_face_to_database <模型目录路径> <人脸ID> <图像路径>
```

示例：
```bash
# 从图像文件添加ID为1的人脸
./add_face_to_database ../model 1 face_image.jpg
```

#### 2.3 从摄像头实时捕获添加（兼容旧版）

```bash
./add_face_to_database <模型目录路径> <人脸ID> camera [摄像头索引]
```

示例：
```bash
# 从默认摄像头捕获并添加ID为2的人脸
./add_face_to_database ../model 2 camera

# 从第二个摄像头捕获并添加ID为3的人脸
./add_face_to_database ../model 3 camera 1
```

## 数据库管理

### 数据库文件

人脸特征数据存储在 SQLite 数据库文件中：
```
database/face_features.db
```

### 数据库特性

- **持久化存储**：程序重启后数据不会丢失
- **自动创建**：首次运行时自动创建数据库文件
- **自动ID分配**：批量导入时自动分配递增ID
- **高效比对**：内置高效的人脸特征比对算法

## 运行演示

项目提供了一键演示脚本：

```bash
./run_demo.sh <模型目录路径> [摄像头索引]
```

演示流程：
1. 构建项目
2. 引导用户添加人脸到数据库
3. 启动实时人脸识别程序

## 故障排除

### 摄像头问题

- 如果默认摄像头无法工作，请尝试不同的摄像头索引（1、2 等）
- 确保 USB 摄像头已正确连接并被系统识别：
  ```bash
  ls /dev/video*
  ```
- 检查摄像头权限：
  ```bash
  sudo chmod 666 /dev/video0
  ```

### 模型加载问题

- 确保模型路径正确并指向包含所需模型文件的目录
- 验证模型与 InspireFace SDK 版本的兼容性
- 检查模型目录中是否包含所有必需的模型文件

### 分段错误

- 通常由模型加载问题或模型不兼容引起
- 尝试使用不同的模型或检查模型文档
- 确保使用正确版本的 InspireFace 库

### 常见错误信息

- `[ERR: InferenceWrapper] Unsupported inference helper type`：模型或库版本不兼容
- `Failed to load model`：检查模型路径和文件完整性

### 数据库问题

- 如果数据库目录无法创建，请检查程序运行目录的写入权限
- 如果数据库文件损坏，可以删除 `database/face_features.db` 文件，程序会自动创建新数据库
