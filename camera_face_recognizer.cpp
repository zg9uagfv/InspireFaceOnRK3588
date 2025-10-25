#include <iostream>
#include <vector>
#include <string>
#include <memory>
#include <opencv2/opencv.hpp>
#include <inspirecv/inspirecv.h>
#include <inspireface/inspireface.hpp>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <sys/stat.h>
#include <unistd.h>

// Function to parse command line arguments
bool ParseArguments(int argc, char** argv, std::string& model_path, int& camera_index) {
    if (argc < 2 || argc > 3) {
        std::cout << "用法: " << argv[0] << " <模型路径> [摄像头索引]" << std::endl;
        std::cout << "  摄像头索引: 0 表示默认摄像头, 1 表示第二个摄像头, 以此类推 (默认: 0)" << std::endl;
        return false;
    }

    model_path = argv[1];
    camera_index = 0;
    
    if (argc == 3) {
        camera_index = std::stoi(argv[2]);
    }
    
    return true;
}

// Function to initialize camera
bool InitializeCamera(cv::VideoCapture& cap, int camera_index) {
    cap.open(camera_index);
    if (!cap.isOpened()) {
        std::cerr << "错误: 无法打开摄像头 " << camera_index << std::endl;
        // Try alternative camera paths for USB cameras
        std::vector<std::string> camera_paths = {
            "/dev/video0", "/dev/video1", "/dev/video2", "/dev/video3"
        };
        
        bool camera_opened = false;
        for (const auto& path : camera_paths) {
            std::cout << "尝试打开 " << path << std::endl;
            cap.open(path);
            if (cap.isOpened()) {
                std::cout << "成功打开 " << path << std::endl;
                camera_opened = true;
                break;
            }
        }
        
        if (!camera_opened) {
            std::cerr << "错误: 无法打开任何摄像头设备" << std::endl;
            return false;
        }
    }

    // Set camera resolution
    cap.set(cv::CAP_PROP_FRAME_WIDTH, 1280);
    cap.set(cv::CAP_PROP_FRAME_HEIGHT, 720);
    
    return true;
}

// Function to load model
bool LoadModel(const std::string& model_path) {
    // Global init(only once)
    auto context = inspire::Launch::GetInstance();
    
    // Try to switch to CPU-based image processing to avoid RGA allocation issues
    // This must be done before loading the model
    auto launch_context = inspire::Launch::GetInstance();
    launch_context->SwitchImageProcessingBackend(
        inspire::Launch::IMAGE_PROCESSING_CPU);
    
    std::cout << "成功切换到CPU图像处理后端" << std::endl;
    
    int load_result = context->Load(model_path);
    if (load_result != 0) {
        std::cerr << "错误: 无法从 " << model_path << " 加载模型 (错误代码: " << load_result << ")" << std::endl;
        std::cerr << "请检查模型路径是否正确以及模型是否兼容." << std::endl;
        return false;
    }

    printf("模型加载成功!!!!!!!!!!\n");
    return true;
}

// Function to create session
std::shared_ptr<inspire::Session> CreateSession() {
    // Create session with face detection and recognition enabled
    inspire::CustomPipelineParameter param;
    param.enable_recognition = true;
	param.enable_liveness = true;
	param.enable_face_quality = true;
    
    std::shared_ptr<inspire::Session> session(
        inspire::Session::CreatePtr(inspire::DETECT_MODE_ALWAYS_DETECT, 1, param, 320));
    
    if (session == nullptr) {
        std::cerr << "错误: 无法创建会话" << std::endl;
    }
    
    return session;
}

// Function to initialize FeatureHubDB
std::shared_ptr<inspire::FeatureHubDB> InitializeFeatureHub() {
    auto feature_hub = inspire::FeatureHubDB::GetInstance();
    inspire::DatabaseConfiguration db_config;
    db_config.enable_persistence = true;  // Enable persistence
    db_config.recognition_threshold = 0.48f;  // Set recognition threshold
    
    // Check if database directory exists
    struct stat info;
    if (stat("database", &info) == 0) {
        db_config.persistence_db_path = "database/face_features.db";
        std::cout << "从数据库文件加载人脸数据: " << db_config.persistence_db_path << std::endl;
    } else {
        std::cout << "警告: 数据库目录不存在，将创建新的数据库" << std::endl;
        // Create database directory
        #if defined(_WIN32)
        _mkdir("database");
        #else
        mkdir("database", 0755);
        #endif
        db_config.persistence_db_path = "database/face_features.db";
    }
    
    int32_t hub_result = feature_hub->EnableHub(db_config);
    if (hub_result != 0) {
        std::cerr << "警告: 无法启用FeatureHubDB (错误代码: " << hub_result << ")" << std::endl;
        // Try without persistence as fallback
        db_config.enable_persistence = false;
        hub_result = feature_hub->EnableHub(db_config);
        if (hub_result != 0) {
            std::cerr << "错误: 无法启用FeatureHubDB，即使禁用持久化 (错误代码: " << hub_result << ")" << std::endl;
            return nullptr;
        }
    } else {
        std::cout << "FeatureHubDB 初始化成功，使用持久化数据库" << std::endl;
        // Print database info
        int32_t face_count = feature_hub->GetFaceFeatureCount();
        std::cout << "数据库中现有人脸数量: " << face_count << std::endl;
        
        // Print all IDs in database for debugging
        if (face_count > 0) {
            auto& existing_ids = feature_hub->GetExistingIds();
            std::cout << "数据库中的人脸ID: ";
            for (const auto& id : existing_ids) {
                std::cout << id << " ";
            }
            std::cout << std::endl;
        }
    }
    
    return feature_hub;
}

// Function to configure session parameters
void ConfigureSession(std::shared_ptr<inspire::Session> session) {
    // Configure face detection threshold (default is typically 0.5)
    // Lower values will detect more faces but may include false positives
    // Higher values will detect fewer faces but with higher confidence
    session->SetFaceDetectThreshold(0.7f);
    
    // Configure minimum face pixel size (default is 0, meaning no minimum)
    // Increase this value to filter out small faces
    session->SetFilterMinimumFacePixelSize(150);
}

// Function to check if GUI is available
bool CheckGUIAvailability() {
    bool gui_available = true;
    try {
        cv::namedWindow("人脸检测", cv::WINDOW_AUTOSIZE);
    } catch (const cv::Exception& e) {
        std::cerr << "警告: GUI不可用, 运行在无头模式下" << std::endl;
        gui_available = false;
    }
    return gui_available;
}

// Function to check if face is frontal
bool IsFrontalFace(const inspire::FaceTrackWrap& face) {
    // Check if the face is frontal (facing forward)
    // For a frontal face: yaw, pitch, and roll should be close to 0
    float yaw = face.face3DAngle.yaw;
    float pitch = face.face3DAngle.pitch;
    float roll = face.face3DAngle.roll;
    
    // Define thresholds for frontal face detection
    const float angle_threshold = 15.0f; // degrees
    return (std::abs(yaw) < angle_threshold) && 
           (std::abs(pitch) < angle_threshold) && 
           (std::abs(roll) < angle_threshold);
}

// Function to compare face with database and return match result
bool CompareFaceWithDatabase(std::shared_ptr<inspire::FeatureHubDB> feature_hub, 
                            const inspire::Embedded& embedding, 
                            cv::Mat& frame, 
                            const inspire::FaceRect& face_rect,
                            int64_t& matched_id,
                            double& similarity) {
    // Print debug info
    std::cout << "开始人脸比对..." << std::endl;
    std::cout << "特征向量维度: " << embedding.size() << std::endl;
    
    // Check database status
    int32_t face_count = feature_hub->GetFaceFeatureCount();
    std::cout << "数据库中人脸数量: " << face_count << std::endl;
    
    if (face_count == 0) {
        std::cout << "警告: 数据库为空，无法进行比对" << std::endl;
        cv::putText(frame, "数据库为空", 
                   cv::Point(face_rect.x, face_rect.y - 30), 
                   cv::FONT_HERSHEY_SIMPLEX, 0.6, 
                   cv::Scalar(0, 0, 255), 2);
        matched_id = -1;
        similarity = 0.0;
        return false;
    }
    
    // Compare with faces in the database
    std::vector<inspire::FaceSearchResult> search_results;
    int32_t search_result = feature_hub->SearchFaceFeatureTopK(embedding, search_results, 3, false);
    std::cout << "比对结果代码: " << search_result << ", 找到匹配数量: " << search_results.size() << std::endl;
    
    if (search_result == 0 && !search_results.empty()) {
        // Get the top match
        auto& top_match = search_results[0];
        matched_id = top_match.id;
        similarity = top_match.similarity;
        
        std::cout << "找到匹配的人脸 - ID: " << matched_id << ", 相似度: " << similarity << std::endl;
        
        // Display match information on the image
        std::string match_info = "匹配ID: " + std::to_string(matched_id);
        std::string similarity_info = "相似度: " + std::to_string(static_cast<int>(similarity * 100)) + "%";
        
        cv::putText(frame, match_info, 
                   cv::Point(face_rect.x, face_rect.y - 30), 
                   cv::FONT_HERSHEY_SIMPLEX, 0.6, 
                   cv::Scalar(255, 0, 0), 2);
        cv::putText(frame, similarity_info, 
                   cv::Point(face_rect.x, face_rect.y - 50), 
                   cv::FONT_HERSHEY_SIMPLEX, 0.6, 
                   similarity > 0.7 ? cv::Scalar(0, 255, 0) : cv::Scalar(0, 165, 255), 2);
        
        // Return true if match is found
        return true;
    } else {
        std::cout << "未找到匹配的人脸" << std::endl;
        std::cout << "搜索结果数量: " << search_results.size() << std::endl;
        cv::putText(frame, "未匹配", 
                   cv::Point(face_rect.x, face_rect.y - 30), 
                   cv::FONT_HERSHEY_SIMPLEX, 0.6, 
                   cv::Scalar(0, 0, 255), 2);
        matched_id = -1;
        similarity = 0.0;
        return false;
    }
}

// Function to save face image with matched ID
void SaveFaceImageWithId(const cv::Mat& frame, const inspire::FaceRect& face_rect, int64_t matched_id) {
    // Save face as JPEG image to pic directory with matched ID in filename
    // Add bounds checking to prevent invalid ROI
    int x = std::max(0, face_rect.x);
    int y = std::max(0, face_rect.y);
    int width = std::min(face_rect.width, frame.cols - x);
    int height = std::min(face_rect.height, frame.rows - y);
    
    // Ensure we have a valid region
    if (width > 0 && height > 0) {
        try {
            // Create a copy of the ROI instead of a reference
            cv::Mat face_img;
            cv::Mat(frame, cv::Rect(x, y, width, height)).copyTo(face_img);
            
            if (!face_img.empty()) {
                // Create filename with matched ID
                auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count();
                std::string filename = "results/face_id_" + std::to_string(matched_id) + "_" + std::to_string(timestamp) + ".jpg";
                std::cout << "尝试保存图像: " << filename << " 尺寸 " << width << "x" << height << std::endl;
                
                // Try different compression parameters
                std::vector<int> compression_params;
                compression_params.push_back(cv::IMWRITE_JPEG_QUALITY);
                compression_params.push_back(90);
                
                bool saved = cv::imwrite(filename, face_img, compression_params);
                if (saved) {
                    std::cout << "保存人脸图像: " << filename << std::endl;
                } else {
                    std::cerr << "无法保存人脸图像: " << filename << std::endl;
                    // Try saving to current directory as fallback
                    std::string fallback_filename = "face_id_" + std::to_string(matched_id) + "_" + std::to_string(timestamp) + ".jpg";
                    bool fallback_saved = cv::imwrite(fallback_filename, face_img, compression_params);
                    if (fallback_saved) {
                        std::cout << "保存人脸图像到当前目录: " << fallback_filename << std::endl;
                    } else {
                        std::cerr << "也无法保存人脸图像到当前目录" << std::endl;
                    }
                }
            } else {
                std::cerr << "无法创建人脸图像ROI副本" << std::endl;
            }
        } catch (const cv::Exception& ex) {
            std::cerr << "保存图像时发生异常: " << ex.what() << std::endl;
        }
    } else {
        std::cerr << "无效的人脸区域: " << face_rect.x << "," << face_rect.y << " " << face_rect.width << "x" << face_rect.height << std::endl;
    }
}

// Function to check if face has glasses with reflections
bool HasGlassesWithReflections(const cv::Mat& frame, const inspire::FaceTrackWrap& face) {
    // This is a simplified implementation for detecting glasses reflections
    // In a real application, you might want to use more sophisticated methods
    
    // Get face rectangle
    auto rect = face.rect;
    
    // Define regions where glasses reflections typically occur
    // These are approximate positions for the lenses area
    int eye_region_y = rect.y + rect.height / 3;  // Roughly where eyes are located
    int eye_height = rect.height / 5;  // Height of eye region
    
    // Left eye region
    int left_eye_x = rect.x + rect.width / 4;
    int left_eye_width = rect.width / 4;
    
    // Right eye region
    int right_eye_x = rect.x + rect.width / 2;
    int right_eye_width = rect.width / 4;
    
    // Extract eye regions
    cv::Rect left_eye_rect(left_eye_x, eye_region_y, left_eye_width, eye_height);
    cv::Rect right_eye_rect(right_eye_x, eye_region_y, right_eye_width, eye_height);
    
    // Ensure regions are within frame bounds
    left_eye_rect &= cv::Rect(0, 0, frame.cols, frame.rows);
    right_eye_rect &= cv::Rect(0, 0, frame.cols, frame.rows);
    
    // Check if regions are valid
    if (left_eye_rect.width <= 0 || left_eye_rect.height <= 0 ||
        right_eye_rect.width <= 0 || right_eye_rect.height <= 0) {
        return false;
    }
    
    // Extract eye regions from frame
    cv::Mat left_eye_region = frame(left_eye_rect);
    cv::Mat right_eye_region = frame(right_eye_rect);
    
    // Convert to grayscale for easier analysis
    cv::Mat left_gray, right_gray;
    cv::cvtColor(left_eye_region, left_gray, cv::COLOR_BGR2GRAY);
    cv::cvtColor(right_eye_region, right_gray, cv::COLOR_BGR2GRAY);
    
    // Apply threshold to detect bright spots (potential reflections)
    cv::Mat left_thresh, right_thresh;
    cv::threshold(left_gray, left_thresh, 200, 255, cv::THRESH_BINARY);
    cv::threshold(right_gray, right_thresh, 200, 255, cv::THRESH_BINARY);
    
    // Count white pixels (bright spots)
    int left_white_pixels = cv::countNonZero(left_thresh);
    int right_white_pixels = cv::countNonZero(right_thresh);
    
    // Calculate the percentage of bright pixels
    double left_ratio = (double)left_white_pixels / (left_eye_rect.width * left_eye_rect.height);
    double right_ratio = (double)right_white_pixels / (right_eye_rect.width * right_eye_rect.height);
    
    // If more than 10% of pixels in either eye region are very bright, 
    // we consider it as potential glasses reflection
    const double reflection_threshold = 0.1;
    bool has_reflection = (left_ratio > reflection_threshold) || (right_ratio > reflection_threshold);
    
    std::cout << "眼镜反光检测 - 左眼反光比例: " << left_ratio << ", 右眼反光比例: " << right_ratio << std::endl;
    
    return has_reflection;
}

// Main function
int main(int argc, char** argv) {
    std::string model_path;
    int camera_index;
    
    // Parse command line arguments
    if (!ParseArguments(argc, argv, model_path, camera_index)) {
        return -1;
    }

    // Initialize OpenCV video capture
    cv::VideoCapture cap;
    if (!InitializeCamera(cap, camera_index)) {
        return -1;
    }

    // Load model
    if (!LoadModel(model_path)) {
        return -1;
    }

    // Create session
    auto session = CreateSession();
    if (session == nullptr) {
        return -1;
    }
    
    // Initialize FeatureHubDB for face recognition comparison
    auto feature_hub = InitializeFeatureHub();
    if (feature_hub == nullptr) {
        return -1;
    }
    
    // Configure session parameters
    ConfigureSession(session);

    cv::Mat frame;
    bool gui_available = CheckGUIAvailability();

    std::cout << "按 'q' 键退出" << std::endl;
    std::cout << "模型成功加载自: " << model_path << std::endl;
    std::cout << "摄像头成功打开, 索引: " << camera_index << std::endl;
    
    // Variables for timing
    auto last_time = std::chrono::high_resolution_clock::now();
    
    while (true) {
        // Capture frame from camera
        cap >> frame;
        if (frame.empty()) {
            std::cerr << "错误: 无法捕获帧" << std::endl;
            break;
        }

        // Convert OpenCV Mat to InspireCV Image
        inspirecv::Image img(frame.cols, frame.rows, 3, frame.data, false);
        inspirecv::FrameProcess process = inspirecv::FrameProcess::Create(
            img.Data(), img.Height(), img.Width(), inspirecv::BGR, inspirecv::ROTATION_0);

        // Detect and track faces
        std::vector<inspire::FaceTrackWrap> results;
        int detect_result = session->FaceDetectAndTrack(process, results);
        if (detect_result != 0) {
            std::cerr << "警告: 人脸检测失败, 错误代码: " << detect_result << std::endl;
        }

        // Calculate and display time interval
        auto current_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(current_time - last_time);
        last_time = current_time;
        
        std::string time_info = "时间间隔: " + std::to_string(duration.count()) + " 毫秒";
        if (gui_available) {
            cv::putText(frame, time_info, 
                       cv::Point(10, 30), 
                       cv::FONT_HERSHEY_SIMPLEX, 0.7, 
                       cv::Scalar(0, 0, 255), 2);
        }

        // Print time interval to console for all modes
        std::cout << "人脸检测时间间隔: " << duration.count() << " 毫秒" << std::endl;

        // Process each detected face
        std::cout << "检测到 " << results.size() << " 张人脸" << std::endl;
        
        // Get face quality confidence for blur detection
        std::vector<float> face_quality_confidence = session->GetFaceQualityConfidence();
        
        for (size_t i = 0; i < results.size(); i++) {
            auto& face = results[i];
            
            // Draw face rectangle
            auto rect = face.rect;
            if (gui_available) {
                cv::rectangle(frame, cv::Rect(rect.x, rect.y, rect.width, rect.height), 
                             cv::Scalar(0, 255, 0), 2);
            }
            
            // Print face detection score/quality
            std::cout << "人脸检测质量分数: ";
            for (int j = 0; j < 5; j++) {
                std::cout << face.quality[j] << " ";
            }
            std::cout << std::endl;
            
            // Check face quality confidence for blur detection
            float quality_score = 0.0f;
            if (i < face_quality_confidence.size()) {
                quality_score = face_quality_confidence[i];
                std::cout << "人脸质量评分 (模糊度检测): " << quality_score << std::endl;
            }
            
            // Check if the face is frontal
            bool is_frontal = IsFrontalFace(face);
            std::string frontal_status = is_frontal ? "正脸" : "非正脸";
            std::cout << "人脸状态: " << frontal_status << std::endl;
            
            // Display frontal status on the image
            if (gui_available) {
                cv::putText(frame, frontal_status, 
                           cv::Point(rect.x, rect.y + rect.height + 20), 
                           cv::FONT_HERSHEY_SIMPLEX, 0.6, 
                           is_frontal ? cv::Scalar(0, 255, 0) : cv::Scalar(0, 0, 255), 2);
            }
            
            // Check for glasses with reflections
            bool has_glasses_reflection = HasGlassesWithReflections(frame, face);
            if (has_glasses_reflection) {
                std::cout << "检测到眼镜反光，可能影响识别质量" << std::endl;
                if (gui_available) {
                    cv::putText(frame, "眼镜反光", 
                               cv::Point(rect.x, rect.y + rect.height + 60), 
                               cv::FONT_HERSHEY_SIMPLEX, 0.6, 
                               cv::Scalar(0, 165, 255), 2);
                }
            }
            
            // Skip face recognition if not frontal
            if (!is_frontal) {
                std::cout << "跳过非正脸的人脸识别" << std::endl;
                continue; // Skip to the next face
            }
            
            // Skip face recognition if face is too blurry (quality score is too low)
            // Quality score ranges from 0.0 (very blurry) to 1.0 (very sharp)
            const float quality_threshold = 0.5f;
            if (quality_score < quality_threshold) {
                std::cout << "跳过模糊人脸的人脸识别 (质量评分: " << quality_score << ")" << std::endl;
                if (gui_available) {
                    cv::putText(frame, "模糊", 
                               cv::Point(rect.x, rect.y + rect.height + 40), 
                               cv::FONT_HERSHEY_SIMPLEX, 0.6, 
                               cv::Scalar(0, 0, 255), 2);
                }
                continue; // Skip to the next face
            }
            
            // Skip face recognition if there are glasses with reflections
            if (has_glasses_reflection) {
                std::cout << "跳过有眼镜反光的人脸识别" << std::endl;
                continue; // Skip to the next face
            }
            
            // Extract face features (only for frontal and sharp faces without glasses reflections)
            inspire::FaceEmbedding feature;
            int extract_result = session->FaceFeatureExtract(process, face, feature);
            if (extract_result != 0) {
                std::cerr << "警告: 人脸特征提取失败, 错误代码: " << extract_result << std::endl;
            } else {
                std::cout << "人脸特征提取成功" << std::endl;
                
                // Compare with faces in the database and get match result
                int64_t matched_id;
                double similarity;
                bool is_matched = CompareFaceWithDatabase(feature_hub, feature.embedding, frame, rect, matched_id, similarity);
                
                // Save face image only if match is found
                if (is_matched && matched_id != -1) {
                    SaveFaceImageWithId(frame, rect, matched_id);
                }
            }
            
            // Display feature vector length (for demonstration) - only if extraction was successful
            if (extract_result == 0 && gui_available) {
                std::string feature_info = "特征维度: " + std::to_string(feature.embedding.size());
                cv::putText(frame, feature_info, 
                           cv::Point(rect.x, rect.y - 10), 
                           cv::FONT_HERSHEY_SIMPLEX, 0.7, 
                           cv::Scalar(0, 255, 0), 2);
            }
            
            // Display quality score on the image
            if (gui_available) {
                std::string quality_info = "质量: " + std::to_string(static_cast<int>(quality_score * 100)) + "%";
                cv::putText(frame, quality_info, 
                           cv::Point(rect.x, rect.y + rect.height + 40), 
                           cv::FONT_HERSHEY_SIMPLEX, 0.6, 
                           quality_score > 0.5 ? cv::Scalar(0, 255, 0) : cv::Scalar(0, 0, 255), 2);
            }
        }

        // Show the frame with detections if GUI is available
        if (gui_available) {
            cv::imshow("人脸检测", frame);
        }
        
        // Check for key press to exit (works in both GUI and headless modes)
        int key = cv::waitKey(1) & 0xFF;
        if (key == 'q' || key == 'Q' || key == 27) {  // 'q' or 'Q' key or ESC key
            std::cout << "检测到退出按键. 正在关闭..." << std::endl;
            break;
        } 

        // Additional headless mode processing
        if (!gui_available) {
            // In headless mode, add a small delay and check for exit condition
            // We'll use a simple counter to occasionally print status
            static int frame_count = 0;
            if (++frame_count % 30 == 0) {
                std::cout << "已处理 " << frame_count << " 帧..." << std::endl;
                std::cout << "时间间隔: " << duration.count() << " 毫秒" << std::endl;
                // For headless mode, we'll break after 3000 frames (about 5 seconds at 60fps)
                // You can modify this condition as needed
                if (frame_count >= 3000) {
                    std::cout << "无头模式下处理3000帧后停止" << std::endl;
                    break;
                }
            }
        }
    }

    // Release resources
    cap.release();
    if (gui_available) {
        cv::destroyAllWindows();
    }
    std::cout << "应用程序成功终止." << std::endl;
    return 0;
}