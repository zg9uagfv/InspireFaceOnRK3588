#include <iostream>
#include <vector>
#include <string>
#include <memory>
#include <opencv2/opencv.hpp>
#include <inspirecv/inspirecv.h>
#include <inspireface/inspireface.hpp>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <algorithm>

/**
 * @brief 从图像目录中提取所有人脸特征并添加到数据库
 * 
 * @param image_dir 图像目录路径
 * @param model_path 模型路径
 * @return int 0表示成功，非0表示失败
 */
int AddFacesFromDirectory(const std::string& image_dir, const std::string& model_path) {
    // Initialize InspireFace
    auto context = inspire::Launch::GetInstance();
    context->SwitchImageProcessingBackend(inspire::Launch::IMAGE_PROCESSING_CPU);
    
    int load_result = context->Load(model_path);
    if (load_result != 0) {
        std::cerr << "错误: 无法加载模型 (错误代码: " << load_result << ")" << std::endl;
        return -1;
    }

    // Create session with face detection and recognition enabled
    inspire::CustomPipelineParameter param;
    param.enable_recognition = true;
    param.enable_face_quality = true;
    
    std::shared_ptr<inspire::Session> session(
        inspire::Session::CreatePtr(inspire::DETECT_MODE_ALWAYS_DETECT, 1, param, 320));
    
    if (session == nullptr) {
        std::cerr << "错误: 无法创建会话" << std::endl;
        return -1;
    }

    // Initialize FeatureHubDB with persistence
    auto feature_hub = inspire::FeatureHubDB::GetInstance();
    inspire::DatabaseConfiguration db_config;
    db_config.enable_persistence = true;  // Enable persistence
    db_config.primary_key_mode = inspire::PrimaryKeyMode::AUTO_INCREMENT;  // Use auto increment ID
    db_config.recognition_threshold = 0.48f;
    
    // Create database directory if it doesn't exist
    struct stat info;
    if (stat("database", &info) != 0) {
        #if defined(_WIN32)
        _mkdir("database");
        #else
        mkdir("database", 0755);
        #endif
        std::cout << "创建数据库目录: database" << std::endl;
    }
    
    db_config.persistence_db_path = "database/face_features.db";
    
    int32_t hub_result = feature_hub->EnableHub(db_config);
    if (hub_result != 0) {
        std::cerr << "错误: 无法启用FeatureHubDB (错误代码: " << hub_result << ")" << std::endl;
        return -1;
    }
    
    // Get current face count in database to determine next ID
    int32_t face_count_before = feature_hub->GetFaceFeatureCount();
    int32_t next_id = face_count_before + 1;
    std::cout << "开始处理目录: " << image_dir << std::endl;
    std::cout << "数据库中现有人脸数量: " << face_count_before << std::endl;
    std::cout << "将从ID " << next_id << " 开始添加" << std::endl;

    // Open directory
    DIR* dir = opendir(image_dir.c_str());
    if (!dir) {
        std::cerr << "错误: 无法打开目录 " << image_dir << std::endl;
        return -1;
    }

    // Read directory entries
    struct dirent* entry;
    std::vector<std::string> image_files;
    
    while ((entry = readdir(dir)) != nullptr) {
        std::string filename = entry->d_name;
        // Check if file has image extension
        std::string ext = filename.substr(filename.find_last_of(".") + 1);
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        
        if (ext == "jpg" || ext == "jpeg" || ext == "png" || ext == "bmp") {
            std::string full_path = image_dir + "/" + filename;
            image_files.push_back(full_path);
            std::cout << "找到图像文件: " << full_path << std::endl;
        }
    }
    
    closedir(dir);
    
    if (image_files.empty()) {
        std::cerr << "目录中未找到图像文件" << std::endl;
        return -1;
    }
    
    std::cout << "总共找到 " << image_files.size() << " 个图像文件" << std::endl;
    
    int success_count = 0;
    // Process each image file
    for (const auto& image_path : image_files) {
        std::cout << "\n处理图像: " << image_path << std::endl;
        
        // Load image
        cv::Mat image = cv::imread(image_path);
        if (image.empty()) {
            std::cerr << "错误: 无法加载图像 " << image_path << std::endl;
            continue;
        }

        // Convert OpenCV Mat to InspireCV Image
        inspirecv::Image img(image.cols, image.rows, 3, image.data, false);
        inspirecv::FrameProcess process = inspirecv::FrameProcess::Create(
            img.Data(), img.Height(), img.Width(), inspirecv::BGR, inspirecv::ROTATION_0);

        // Detect faces
        std::vector<inspire::FaceTrackWrap> faces;
        int detect_result = session->FaceDetectAndTrack(process, faces);
        if (detect_result != 0) {
            std::cerr << "警告: 人脸检测失败, 错误代码: " << detect_result << std::endl;
            continue;
        }

        if (faces.empty()) {
            std::cerr << "错误: 图像中未检测到人脸 " << image_path << std::endl;
            continue;
        }

        std::cout << "在 " << image_path << " 中检测到 " << faces.size() << " 张人脸" << std::endl;

        // Process the first detected face
        auto& face = faces[0];
        
        // Check if the face is frontal
        float yaw = face.face3DAngle.yaw;
        float pitch = face.face3DAngle.pitch;
        float roll = face.face3DAngle.roll;
        const float angle_threshold = 15.0f;
        bool is_frontal = (std::abs(yaw) < angle_threshold) && 
                          (std::abs(pitch) < angle_threshold) && 
                          (std::abs(roll) < angle_threshold);
        
        if (!is_frontal) {
            std::cerr << "警告: 检测到的人脸不是正脸，可能影响识别效果" << std::endl;
            std::cout << "人脸角度 - 偏航角: " << yaw << ", 俯仰角: " << pitch << ", 翻滚角: " << roll << std::endl;
        }
        
        // Extract face features
        inspire::FaceEmbedding feature;
        int extract_result = session->FaceFeatureExtract(process, face, feature);
        if (extract_result != 0) {
            std::cerr << "错误: 人脸特征提取失败, 错误代码: " << extract_result << std::endl;
            continue;
        }

        std::cout << "人脸特征提取成功，特征维度: " << feature.embedding.size() << std::endl;

        // Add face feature to database with auto increment ID
        int64_t result_id;
        std::vector<float> feature_vector(feature.embedding.begin(), feature.embedding.end());
        int32_t insert_result = feature_hub->FaceFeatureInsert(feature_vector, -1, result_id);  // -1 for auto increment
        
        if (insert_result == 0) {
            std::cout << "成功将人脸特征添加到数据库，ID: " << result_id << std::endl;
            success_count++;
        } else {
            std::cerr << "错误: 无法将人脸特征添加到数据库 (错误代码: " << insert_result << ")" << std::endl;
        }
    }
    
    // Print final database status
    int32_t face_count_after = feature_hub->GetFaceFeatureCount();
    std::cout << "\n处理完成!" << std::endl;
    std::cout << "成功添加 " << success_count << " 个人脸特征到数据库" << std::endl;
    std::cout << "数据库中现有人脸数量: " << face_count_after << std::endl;
    
    // Print all IDs in database
    if (face_count_after > 0) {
        auto& existing_ids = feature_hub->GetExistingIds();
        std::cout << "数据库中的人脸ID: ";
        for (const auto& id : existing_ids) {
            std::cout << id << " ";
        }
        std::cout << std::endl;
    }
    
    return (success_count > 0) ? 0 : -1;
}

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cout << "用法: " << argv[0] << " <模型路径> [图像目录]" << std::endl;
        std::cout << "  如果提供图像目录，则从目录中所有图像提取人脸特征" << std::endl;
        std::cout << "示例:" << std::endl;
        std::cout << "  " << argv[0] << " ../model /path/to/image/directory" << std::endl;
        return -1;
    }

    std::string model_path = argv[1];

    if (argc >= 2) {
        // Check if argv[2] is a directory
        struct stat info;
        if (stat(argv[2], &info) == 0 && info.st_mode & S_IFDIR) {
            // It's a directory, process all images in the directory
            return AddFacesFromDirectory(argv[2], model_path);
        } else {
			std::cout<<"输入的参数不是目录"<<std::endl;
			return -1;
		}
    } else {
        std::cout << "参数错误，请检查用法" << std::endl;
        return -1;
    }
}