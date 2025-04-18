/*
 * Copyright 2017 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "hello_ar_application.h"

#include <android/asset_manager.h>
#include <jni.h>

#include <array>

#include "include/arcore/arcore_c_api.h"
#include "plane_renderer.h"
#include "line_renderer.h"
#include "util.h"

namespace hello_ar {
    namespace {
        constexpr size_t kMaxNumberOfAndroidsToRender = 20;

// Assumed distance from the device camera to the surface on which user will
// try to place objects. This value affects the apparent scale of objects
// while the tracking method of the Instant Placement point is
// SCREENSPACE_WITH_APPROXIMATE_DISTANCE. Values in the [0.2, 2.0] meter
// range are a good choice for most AR experiences. Use lower values for AR
// experiences where users are expected to place objects on surfaces close
// to the camera. Use larger values for experiences where the user will
// likely be standing and trying to place an object on the ground or floor
// in front of them.
        constexpr float kApproximateDistanceMeters = 1.0f;

        void SetColor(float r, float g, float b, float a, float* color4f) {
            color4f[0] = r;
            color4f[1] = g;
            color4f[2] = b;
            color4f[3] = a;
        }

    }  // namespace

    HelloArApplication::HelloArApplication(AAssetManager* asset_manager)
            : asset_manager_(asset_manager),
            direction_match_count_(0),         // ⭐ 방향 일치 카운트 초기화
            direction_check_enabled_(true)     // ⭐ 방향 체크는 처음엔 활성화
        {}

    HelloArApplication::~HelloArApplication() {
        if (ar_session_ != nullptr) {
            ArSession_destroy(ar_session_);
            ArFrame_destroy(ar_frame_);
        }
    }

    JNIEnv* HelloArApplication::GetJniEnv() {
        JNIEnv* env = nullptr;
        if (java_vm_ && java_vm_->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) == JNI_OK) {
            return env;
        }
        return nullptr;
    }


    void HelloArApplication::TryGeneratePathIfNeeded(float cam_x, float cam_z) {
        if (path_generated_) return;
    
        Point start = {cam_x, cam_z};
        Point goal = {-10.0f, -18.0f}; // 목적지는 고정되어 있음
    
        std::vector<Point> outer_rect = {
            {-11.5f, 1.8f}, {-11.5f, -20.25f}, {1.5f, -20.25f}, {1.5f, 1.8f}
        };
        std::vector<Point> inner_rect = {
            {-8.58f, -0.6f}, {-8.58f, -15.89f}, {-1.49f, -15.89f}, {-1.49f, -0.6f}
        };
    
        std::set<Point> obstacles;
        for (int i = 0; i < outer_rect.size(); ++i) {
            auto wall = generateWall(outer_rect[i], outer_rect[(i + 1) % outer_rect.size()]);
            obstacles.insert(wall.begin(), wall.end());
        }
        for (int i = 0; i < inner_rect.size(); ++i) {
            auto wall = generateWall(inner_rect[i], inner_rect[(i + 1) % inner_rect.size()]);
            obstacles.insert(wall.begin(), wall.end());
        }
    
        path = astar(start, goal, obstacles);
        if (!path.empty()) {
            path_generated_ = true;
            path_ready_to_render_ = true;
            arrival_audio_played_ = false;
            LOGI("🚀 경로 탐색 성공! A* 결과:");

            if (!start_flag){
                JNIEnv* env = GetJniEnv();
                audio::PlayAudioFromAssets(env, "start.m4a");
                start_flag = true;
                LOGI("start.m4a 재생 성공");
            }

//                JNIEnv* env = GetJniEnv();
//                jclass clazz = env->FindClass("com/capstone/whereigo/HelloArFragment");
//                jmethodID ttsMethod = env->GetStaticMethodID(clazz, "playTTS", "(Ljava/lang/String;)V");
//
//                if (clazz != nullptr && ttsMethod != nullptr) {
//                    jstring message = env->NewStringUTF("경로 안내를 시작합니다.");
//                    env->CallStaticVoidMethod(clazz, ttsMethod, message);
//                    env->DeleteLocalRef(message);
//                }
        }
        else {
            LOGI("❌ 경로 탐색 실패: 도달 불가능");
        }
    }
    

    void HelloArApplication::CheckCameraFollowingPath(float* pose_raw, float cam_x, float cam_z) {
        if (current_path_index >= path.size()) {
            LOGI("🎉 모든 경로를 성공적으로 따라갔습니다!");

            JNIEnv* env = GetJniEnv();
            jclass clazz = env->FindClass("com/capstone/whereigo/HelloArFragment");
            jmethodID method = env->GetStaticMethodID(clazz, "updatePathStatusFromNative", "(Ljava/lang/String;)V");

            jstring message = env->NewStringUTF("🎉 모든 경로를 따라갔습니다!");
            env->CallStaticVoidMethod(clazz, method, message);
            env->DeleteLocalRef(message);
            if (env) {
                audio::PlayAudioFromAssets(env, "arrival.m4a");
                arrival_audio_played_ = true;
            }

            arrival_audio_played_ = true;
            return;

//            if (tts_arrival_played_) return;
//
//            JNIEnv* env = GetJniEnv();
//            jclass clazz = env->FindClass("com/capstone/whereigo/HelloArFragment");
//            jmethodID method = env->GetStaticMethodID(clazz, "updatePathStatusFromNative", "(Ljava/lang/String;)V");
//
//            jstring message = env->NewStringUTF("🎉 모든 경로를 따라갔습니다!");
//            env->CallStaticVoidMethod(clazz, method, message);
//            env->DeleteLocalRef(message);
        }

        if (current_path_index >= path.size()) return;

        Point target = path[current_path_index];
        float dx = cam_x - target.x;
        float dz = cam_z - target.z;
        float distance = std::sqrt(dx * dx + dz * dz);


        const float deviation_threshold = 2.0f;  // 2m 이상 벗어나면 재탐색

        if (distance > deviation_threshold) {
            LOGI("🚨 경로 이탈 감지됨! 새 경로를 재탐색합니다.");

            JNIEnv* env = GetJniEnv();
            if (env) {
                audio::PlayAudioFromAssets(env, "deviation.m4a");
            }

            path.clear();
            path_generated_ = false;  // ⭐ 경로 재생성을 허용
            path_ready_to_render_ = false;
            current_path_index = 0;

            // 기존 목표점으로 재탐색 시도
            TryGeneratePathIfNeeded(cam_x, cam_z);
    
            if (!path.empty()) {
                JNIEnv* env = GetJniEnv();
                jclass clazz = env->FindClass("com/capstone/whereigo/HelloArFragment");
                jmethodID method = env->GetStaticMethodID(clazz, "updatePathStatusFromNative", "(Ljava/lang/String;)V");
                jstring msg = env->NewStringUTF("🚨 경로 이탈 - 새 경로 탐색 완료");
                env->CallStaticVoidMethod(clazz, method, msg);
                env->DeleteLocalRef(msg);
            } else {
                LOGI("❌ 경로 재탐색 실패: 도달할 수 없음");
            }
    
            return;
        }
        
        std::string status;
        char buffer[128];

        if (distance < threshold) {
            CheckDirectionToNextNode(pose_raw, {cam_x, cam_z}, target);
            snprintf(buffer, sizeof(buffer), "✅ 경로 지점 %d 도달 (x=%.2f, z=%.2f)", current_path_index, target.x, target.z);
            // ✅ 방향 체크 다시 활성화
            direction_check_enabled_ = true;
            direction_match_count_ = 0;
            current_path_index++;
        } else {
            CheckDirectionToNextNode(pose_raw, {cam_x, cam_z}, target);
            snprintf(buffer, sizeof(buffer), "📍 경로 %d 접근 중... x 방향: %.2f m, z 방향: %.2f m",
                     current_path_index, dx, dz);
        }

        JNIEnv* env = GetJniEnv();
        jclass clazz = env->FindClass("com/capstone/whereigo/HelloArFragment");
        jmethodID method = env->GetStaticMethodID(clazz, "updatePathStatusFromNative", "(Ljava/lang/String;)V");

        jstring message = env->NewStringUTF(buffer);
        env->CallStaticVoidMethod(clazz, method, message);
        env->DeleteLocalRef(message);
    }

    void HelloArApplication::CheckDirectionToNextNode(float* pose_raw, const Point& cam_position, const Point& target_node) {
        ArPose* camera_pose;
        ArPose_create(ar_session_, nullptr, &camera_pose);
        ArCamera* ar_camera = nullptr;
        ArFrame_acquireCamera(ar_session_, ar_frame_, &ar_camera);
        ArCamera_getPose(ar_session_, ar_camera, camera_pose);

        float matrix[16];
        ArPose_getMatrix(ar_session_, camera_pose, matrix);
        glm::vec3 forward(-matrix[8], -matrix[9], -matrix[10]);
        float yawRad = std::atan2(forward.x, forward.z);
        float yawDeg = glm::degrees(yawRad);
        if (yawDeg < 0) yawDeg += 360.0f;

        float dx = target_node.x - cam_position.x;
        float dz = target_node.z - cam_position.z;
        float pathDeg = std::atan2(dx, dz) * 180.0f / M_PI;
        if (pathDeg < 0) pathDeg += 360.0f;

        float angle_diff = std::fabs(yawDeg - pathDeg);
        if (angle_diff > 180.0f) angle_diff = 360.0f - angle_diff;

        if (direction_check_enabled_) {
            if (angle_diff < 25.0f) {
                direction_match_count_++;
                if (direction_match_count_ >= 10) {
                    direction_check_enabled_ = false;
                    LOGI("🟢 방향 일치 10회 연속 → 방향 체크 중단");
                    JNIEnv* env = GetJniEnv();
                    if (env) {
                        jclass clazz = env->FindClass("com/capstone/whereigo/HelloArFragment");
                        jmethodID vibrateMethod = env->GetStaticMethodID(clazz, "vibrateOnce", "()V");
                        if (vibrateMethod != nullptr) {
                            env->CallStaticVoidMethod(clazz, vibrateMethod);
                        }
                    }
                } else {
                    LOGI("🟢 방향 일치 (%d회): camera=%.1f°, path=%.1f°, diff=%.1f°", direction_match_count_, yawDeg, pathDeg, angle_diff);
                }
            } else {
                direction_match_count_ = 0;
                LOGI("🔄 방향 불일치: camera=%.1f°, path=%.1f°, diff=%.1f°", yawDeg, pathDeg, angle_diff);
            }
        } else {
            // 틀어진 경우 체크 재개
            if (angle_diff > 25.0f) {
                direction_check_enabled_ = true;
                direction_match_count_ = 0;
                LOGI("🔁 방향 틀어짐 %.1f° → 방향 체크 재시작", angle_diff);
            }
        }

        JNIEnv* env = GetJniEnv();
        if (env) {
            jclass clazz = env->FindClass("com/capstone/whereigo/HelloArFragment");
            jmethodID method = env->GetStaticMethodID(clazz, "updateYawFromNative", "(FF)V");
            if (method != nullptr) {
                env->CallStaticVoidMethod(clazz, method, yawDeg, pathDeg);
            }
        }

        ArCamera_release(ar_camera);
        ArPose_destroy(camera_pose);
    }

    

    void HelloArApplication::OnPause() {
        LOGI("OnPause()");
        if (ar_session_ != nullptr) {
            ArSession_pause(ar_session_);
        }
    }

    void HelloArApplication::OnResume(JNIEnv* env, void* context, void* activity) {
        LOGI("OnResume()");

        env->GetJavaVM(&java_vm_);


        if (ar_session_ == nullptr) {
            ArInstallStatus install_status;
            // If install was not yet requested, that means that we are resuming the
            // activity first time because of explicit user interaction (such as
            // launching the application)
            bool user_requested_install = !install_requested_;

            // === ATTENTION!  ATTENTION!  ATTENTION! ===
            // This method can and will fail in user-facing situations.  Your
            // application must handle these cases at least somewhat gracefully.  See
            // HelloAR Java sample code for reasonable behavior.
            CHECKANDTHROW(
                    ArCoreApk_requestInstall(env, activity, user_requested_install,
                                             &install_status) == AR_SUCCESS,
                    env, "Please install Google Play Services for AR (ARCore).");

            switch (install_status) {
                case AR_INSTALL_STATUS_INSTALLED:
                    break;
                case AR_INSTALL_STATUS_INSTALL_REQUESTED:
                    install_requested_ = true;
                    return;
            }

            // === ATTENTION!  ATTENTION!  ATTENTION! ===
            // This method can and will fail in user-facing situations.  Your
            // application must handle these cases at least somewhat gracefully.  See
            // HelloAR Java sample code for reasonable behavior.
            CHECKANDTHROW(ArSession_create(env, context, &ar_session_) == AR_SUCCESS,
                          env, "Failed to create AR session.");

            ConfigureSession();
            ArFrame_create(ar_session_, &ar_frame_);

            ArSession_setDisplayGeometry(ar_session_, display_rotation_, width_,
                                         height_);
        }

        const ArStatus status = ArSession_resume(ar_session_);
        CHECKANDTHROW(status == AR_SUCCESS, env, "Failed to resume AR session.");
    }

    void HelloArApplication::OnSurfaceCreated() {
        LOGI("OnSurfaceCreated()");

        depth_texture_.CreateOnGlThread();
        background_renderer_.InitializeGlContent(asset_manager_,
                                                 depth_texture_.GetTextureId());
        point_cloud_renderer_.InitializeGlContent(asset_manager_);
        andy_renderer_.InitializeGlContent(asset_manager_, "models/andy.obj",
                                           "models/andy.png");
        andy_renderer_.SetDepthTexture(depth_texture_.GetTextureId(),
                                       depth_texture_.GetWidth(),
                                       depth_texture_.GetHeight());
        location_pin_renderer_.InitializeGlContent(asset_manager_, "models/location_pin.obj", "models/location_pin.png");
        plane_renderer_.InitializeGlContent(asset_manager_);
        arrow_renderer_.InitializeGlContent(asset_manager_, "models/arrow.obj", "models/arrow.png");
        car_arrow_renderer_.InitializeGlContent(asset_manager_, "models/carArrow.obj", "models/carArrow.png");
        line_renderer_.InitializeGlContent(asset_manager_);
    }

    void HelloArApplication::OnDisplayGeometryChanged(int display_rotation,
                                                      int width, int height) {
        LOGI("OnSurfaceChanged(%d, %d)", width, height);
        glViewport(0, 0, width, height);
        display_rotation_ = display_rotation;
        width_ = width;
        height_ = height;
        if (ar_session_ != nullptr) {
            ArSession_setDisplayGeometry(ar_session_, display_rotation, width, height);
        }
    }

    void HelloArApplication::OnDrawFrame(bool depthColorVisualizationEnabled,
                                         bool useDepthForOcclusion) {
        // Render the scene.
        glClearColor(0.9f, 0.9f, 0.9f, 1.0f);
        glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);

        glEnable(GL_CULL_FACE);
        glEnable(GL_DEPTH_TEST);

        if (ar_session_ == nullptr) return;

        ArSession_setCameraTextureName(ar_session_,
                                       background_renderer_.GetTextureId());

        // Update session to get current frame and render camera background.
        if (ArSession_update(ar_session_, ar_frame_) != AR_SUCCESS) {
            LOGE("HelloArApplication::OnDrawFrame ArSession_update error");
        }

        andy_renderer_.SetDepthTexture(depth_texture_.GetTextureId(),
                                       depth_texture_.GetWidth(),
                                       depth_texture_.GetHeight());

        ArCamera* ar_camera = nullptr;
        ArFrame_acquireCamera(ar_session_, ar_frame_, &ar_camera);

        // 🔧 [1] 카메라 트래킹 상태 확인
        ArTrackingState camera_tracking_state;
        ArCamera_getTrackingState(ar_session_, ar_camera, &camera_tracking_state);

        if (camera_tracking_state != AR_TRACKING_STATE_TRACKING) {
            LOGI("⚠️ 카메라 트래킹 안됨 - 앵커 및 경로 생성 생략");
        }

        // 🔧 [2] 카메라 Pose 추출
        ArPose* camera_pose;
        ArPose_create(ar_session_, nullptr, &camera_pose);
        float pose_raw[7];
        ArCamera_getPose(ar_session_, ar_camera, camera_pose);
        ArPose_getPoseRaw(ar_session_, camera_pose, pose_raw);

        float cam_x = pose_raw[4];
        float cam_z = pose_raw[6];

        // 5. 경로 생성 시도
        TryGeneratePathIfNeeded(cam_x, cam_z);

        // 6. 경로 따라가기
        if (!path.empty()) {
            CheckCameraFollowingPath(pose_raw, cam_x, cam_z);
        }

        // [추가] Java로 pose 값을 전달
        JavaVM* java_vm;
        JNIEnv* env = nullptr;

        if (java_vm_ && java_vm_->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) == JNI_OK) {
            jfloatArray pose_array = env->NewFloatArray(7);
            env->SetFloatArrayRegion(pose_array, 0, 7, pose_raw);

            jclass clazz = env->FindClass("com/capstone/whereigo/HelloArFragment");
            jmethodID method = env->GetStaticMethodID(clazz, "updatePoseFromNative", "([F)V");

            if (clazz != nullptr && method != nullptr) {
                env->CallStaticVoidMethod(clazz, method, pose_array);
            }
        }
        // 6. Pose 객체 해제
        ArPose_destroy(camera_pose);
        ArCamera_release(ar_camera);

        int32_t geometry_changed = 0;
        ArFrame_getDisplayGeometryChanged(ar_session_, ar_frame_, &geometry_changed);
        if (geometry_changed != 0 || !calculate_uv_transform_) {
            // The UV Transform represents the transformation between screenspace in
            // normalized units and screenspace in units of pixels.  Having the size of
            // each pixel is necessary in the virtual object shader, to perform
            // kernel-based blur effects.
            calculate_uv_transform_ = false;
            glm::mat3 transform = GetTextureTransformMatrix(ar_session_, ar_frame_);
            andy_renderer_.SetUvTransformMatrix(transform);
        }

        glm::mat4 view_mat;
        glm::mat4 projection_mat;
        ArCamera_getViewMatrix(ar_session_, ar_camera, glm::value_ptr(view_mat));
        ArCamera_getProjectionMatrix(ar_session_, ar_camera,
                /*near=*/0.1f, /*far=*/100.f,
                                     glm::value_ptr(projection_mat));


        background_renderer_.Draw(ar_session_, ar_frame_,
                                  depthColorVisualizationEnabled);

        // line
        if (!path.empty()) {
            std::vector<glm::vec3> line_points;
            for (const auto& p : path) {
                line_points.emplace_back(p.x, plane_y_, p.z);
            }

            line_renderer_.Draw(line_points, projection_mat, view_mat);
        }

        const float green_arrow_color_correction[4] = {0.8f, 0.9f, 0.3f, 1.0f};
        ColoredAnchor arrow_colored_anchor;
        arrow_colored_anchor.anchor = nullptr;
        arrow_colored_anchor.trackable = nullptr;
        SetColor(0.8f, 0.9f, 0.3f, 1.0f, arrow_colored_anchor.color);

        if (!path.empty()) {
            for (size_t i = 0; i < arrow_anchors_.size(); ++i) {
                if (i >= path.size() - 1) continue;

                const ColoredAnchor& arrow_anchor = arrow_anchors_[i];

                Point from = path[i];
                Point to = path[i + 1];

                glm::vec3 direction(to.x - from.x, 0.0f, to.z - from.z);
                float length = glm::length(direction);
                if (length < 0.01f) continue;

                direction = glm::normalize(direction);
                float angle = std::atan2(direction.x, direction.z);
                angle -= glm::pi<float>();

                glm::mat4 rotation_mat = glm::rotate(glm::mat4(1.0f), angle, glm::vec3(0.0f, 1.0f, 0.0f));

                // 📍 중간 위치 계산
                glm::vec3 mid_pos((from.x + to.x) * 0.5f, plane_y_, (from.z + to.z) * 0.5f);

                // 📍 카메라 위치
                glm::vec3 camera_pos(cam_x, plane_y_, cam_z); // 평면 기준으로 y는 맞춤

                // 📏 평면 거리
                float camera_distance = glm::length(mid_pos - camera_pos);

                // 📏 수직 높이 차이 (추가로 반영하면 더 정밀함)
                float height_diff = std::abs(plane_y_ - pose_raw[5]);

                // 🎯 최종 스케일 보정: 거리와 높이 반영
                float dynamic_scale = length * glm::clamp(1.0f / (camera_distance + 0.5f + height_diff), 0.15f, 1.0f);


                glm::vec3 scale_size(0.1f, dynamic_scale, 0.1f);

                glm::mat4 scale_mat = glm::scale(glm::mat4(1.0f), scale_size);

                glm::mat4 model_mat(1.0f);
                util::GetTransformMatrixFromAnchor(*arrow_anchor.anchor, ar_session_, &model_mat);
                model_mat = model_mat *rotation_mat* scale_mat;

                //arrow_renderer_.Draw(projection_mat, view_mat, model_mat,
                //                     green_arrow_color_correction, arrow_anchor.color);
            }
        }

        //ArTrackingState camera_tracking_state;
        ArCamera_getTrackingState(ar_session_, ar_camera, &camera_tracking_state);

        // If the camera isn't tracking don't bother rendering other objects.
        if (camera_tracking_state != AR_TRACKING_STATE_TRACKING) {
            LOGI("⚠️ 카메라 트래킹 안됨 - 앵커 생성 불가");
            return;
        }

        int32_t is_depth_supported = 0;
        ArSession_isDepthModeSupported(ar_session_, AR_DEPTH_MODE_AUTOMATIC,
                                       &is_depth_supported);
        if (is_depth_supported) {
            depth_texture_.UpdateWithDepthImageOnGlThread(*ar_session_, *ar_frame_);
        }

        // Get light estimation value.
        ArLightEstimate* ar_light_estimate;
        ArLightEstimateState ar_light_estimate_state;
        ArLightEstimate_create(ar_session_, &ar_light_estimate);

        ArFrame_getLightEstimate(ar_session_, ar_frame_, ar_light_estimate);
        ArLightEstimate_getState(ar_session_, ar_light_estimate,
                                 &ar_light_estimate_state);

        // Set light intensity to default. Intensity value ranges from 0.0f to 1.0f.
        // The first three components are color scaling factors.
        // The last one is the average pixel intensity in gamma space.
        float color_correction[4] = {1.f, 1.f, 1.f, 1.f};
        if (ar_light_estimate_state == AR_LIGHT_ESTIMATE_STATE_VALID) {
            ArLightEstimate_getColorCorrection(ar_session_, ar_light_estimate,
                                               color_correction);
        }

        ArLightEstimate_destroy(ar_light_estimate);
        ar_light_estimate = nullptr;

        // Update and render planes.
        ArTrackableList* plane_list = nullptr;
        ArTrackableList_create(ar_session_, &plane_list);
        CHECK(plane_list != nullptr);

        ArTrackableType plane_tracked_type = AR_TRACKABLE_PLANE;
        ArSession_getAllTrackables(ar_session_, plane_tracked_type, plane_list);

        int32_t plane_list_size = 0;
        ArTrackableList_getSize(ar_session_, plane_list, &plane_list_size);
        plane_count_ = plane_list_size;

        if (path_ready_to_render_) {

            for (auto& anchor : anchors_) {
                if (anchor.anchor != nullptr) ArAnchor_release(anchor.anchor);
                if (anchor.trackable != nullptr) ArTrackable_release(anchor.trackable);
            }
            anchors_.clear();

            for (auto& anchor : arrow_anchors_) {
                if (anchor.anchor != nullptr) ArAnchor_release(anchor.anchor);
                if (anchor.trackable != nullptr) ArTrackable_release(anchor.trackable);
            }
            arrow_anchors_.clear();

            for (auto& anchor : carArrow_anchors_) {
                if (anchor.anchor != nullptr) ArAnchor_release(anchor.anchor);
                if (anchor.trackable != nullptr) ArTrackable_release(anchor.trackable);
            }
            carArrow_anchors_.clear();

            for (size_t i = 0; i < path.size() - 1; ++i) {
                const Point& p = path[i];
            
                float anchor_pose[7] = {0};
                anchor_pose[4] = p.x;
                anchor_pose[5] = plane_y_;  // 평면 높이로 고정
                anchor_pose[6] = p.z;
            
                ArPose* pose = nullptr;
                ArPose_create(ar_session_, anchor_pose, &pose);
            
                ArAnchor* anchor = nullptr;
                if (ArSession_acquireNewAnchor(ar_session_, pose, &anchor) == AR_SUCCESS) {
                    ColoredAnchor car_anchor;
                    car_anchor.anchor = anchor;
                    car_anchor.trackable = nullptr;
                    SetColor(1.0f, 1.0f, 1.0f, 1.0f, car_anchor.color);  // 흰색 또는 원하는 색
                    carArrow_anchors_.push_back(car_anchor);
                }
            
                ArPose_destroy(pose);
            }

            const auto& p = path.back();
            float anchor_pose[7] = {0};
            anchor_pose[4] = p.x;
            anchor_pose[5] = plane_y_;
            anchor_pose[6] = p.z;

            ArPose* pose = nullptr;
            ArPose_create(ar_session_, anchor_pose, &pose);

            ArAnchor* anchor = nullptr;
            if (ArSession_acquireNewAnchor(ar_session_, pose, &anchor) == AR_SUCCESS) {
                ColoredAnchor colored_anchor;
                colored_anchor.anchor = anchor;
                colored_anchor.trackable = nullptr;
                SetColor(255, 0, 0, 255, colored_anchor.color);
                anchors_.push_back(colored_anchor);
                LOGI("✅ 앵커 생성: x=%.2f, z=%.2f", p.x, p.z);
            }
            ArPose_destroy(pose);

            path_ready_to_render_ = false;  // 앵커 생성 완료
        }


        for (int i = 0; i < plane_list_size; ++i) {
            ArTrackable* ar_trackable = nullptr;
            ArTrackableList_acquireItem(ar_session_, plane_list, i, &ar_trackable);
            ArPlane* ar_plane = ArAsPlane(ar_trackable);
            ArTrackingState out_tracking_state;
            ArTrackable_getTrackingState(ar_session_, ar_trackable,
                                         &out_tracking_state);

            ArPlane* subsume_plane;
            ArPlane_acquireSubsumedBy(ar_session_, ar_plane, &subsume_plane);
            if (subsume_plane != nullptr) {
                ArTrackable_release(ArAsTrackable(subsume_plane));
                ArTrackable_release(ar_trackable);
                continue;
            }

            if (ArTrackingState::AR_TRACKING_STATE_TRACKING != out_tracking_state) {
                ArTrackable_release(ar_trackable);
                continue;
            }

            plane_renderer_.Draw(projection_mat, view_mat, *ar_session_, *ar_plane);
            ArTrackable_release(ar_trackable);
        }

        ArTrackableList_destroy(plane_list);
        plane_list = nullptr;

        andy_renderer_.setUseDepthForOcclusion(asset_manager_, useDepthForOcclusion);

        // Render Andy objects.
        glm::mat4 model_mat(1.0f);
        for (auto& colored_anchor : anchors_) {
            // 🔧 수정: trackable이 nullptr일 경우 UpdateAnchorColor 생략
            if (colored_anchor.trackable != nullptr) {
                UpdateAnchorColor(&colored_anchor);
            }

            // 무조건 렌더
            util::GetTransformMatrixFromAnchor(*colored_anchor.anchor, ar_session_,
                                               &model_mat);
            location_pin_renderer_.Draw(projection_mat, view_mat, model_mat, color_correction,
                                colored_anchor.color);
        }

        for (size_t i = 0; i < carArrow_anchors_.size(); ++i) {
            if (i >= path.size()) continue;
        
            // 👉 경로 시작점
            const Point& from = path[i];
        
            // 👉 도착점이 있으면 방향 계산 (마지막 점은 생략 가능)
            Point to = (i + 1 < path.size()) ? path[i + 1] : from;
        
            // 방향 벡터 계산
            glm::vec3 direction(to.x - from.x, 0.0f, to.z - from.z);
            float length = glm::length(direction);
            if (length < 0.01f) continue;
        
            direction = glm::normalize(direction);
            float angle = std::atan2(direction.x, direction.z) - glm::pi<float>();
        
            // 👉 위치는 path[i]로, y축은 stored_plane_y_로 고정
            glm::vec3 position(from.x, plane_y_, from.z);
        
            // 👉 모델 행렬 구성
            glm::mat4 model_mat = glm::translate(glm::mat4(1.0f), position);
            glm::mat4 rotation_mat = glm::rotate(glm::mat4(1.0f), angle, glm::vec3(0, 1, 0));
            glm::mat4 scale_mat = glm::scale(glm::mat4(1.0f), glm::vec3(0.05f));
        
            model_mat = model_mat * rotation_mat * scale_mat;
        
            // 👉 렌더링
            const ColoredAnchor& car_anchor = carArrow_anchors_[i];
            car_arrow_renderer_.Draw(projection_mat, view_mat, model_mat, color_correction, car_anchor.color);
        }
        



        // Update and render point cloud.
        ArPointCloud* ar_point_cloud = nullptr;
        ArStatus point_cloud_status =
                ArFrame_acquirePointCloud(ar_session_, ar_frame_, &ar_point_cloud);
        if (point_cloud_status == AR_SUCCESS) {
            point_cloud_renderer_.Draw(projection_mat * view_mat, ar_session_,
                                       ar_point_cloud);
            ArPointCloud_release(ar_point_cloud);
        }
    }

    bool HelloArApplication::IsDepthSupported() {
        int32_t is_supported = 0;
        ArSession_isDepthModeSupported(ar_session_, AR_DEPTH_MODE_AUTOMATIC,
                                       &is_supported);
        return is_supported;
    }

    void HelloArApplication::ConfigureSession() {
        const bool is_depth_supported = IsDepthSupported();

        ArConfig* ar_config = nullptr;
        ArConfig_create(ar_session_, &ar_config);
        if (is_depth_supported) {
            ArConfig_setDepthMode(ar_session_, ar_config, AR_DEPTH_MODE_AUTOMATIC);
        } else {
            ArConfig_setDepthMode(ar_session_, ar_config, AR_DEPTH_MODE_DISABLED);
        }

        if (is_instant_placement_enabled_) {
            ArConfig_setInstantPlacementMode(ar_session_, ar_config,
                                             AR_INSTANT_PLACEMENT_MODE_LOCAL_Y_UP);
        } else {
            ArConfig_setInstantPlacementMode(ar_session_, ar_config,
                                             AR_INSTANT_PLACEMENT_MODE_DISABLED);
        }
        CHECK(ar_config);
        CHECK(ArSession_configure(ar_session_, ar_config) == AR_SUCCESS);
        ArConfig_destroy(ar_config);
    }

    void HelloArApplication::OnSettingsChange(bool is_instant_placement_enabled) {
        is_instant_placement_enabled_ = is_instant_placement_enabled;

        if (ar_session_ != nullptr) {
            ConfigureSession();
        }
    }

    void HelloArApplication::OnTouched(float x, float y) {
        return;
        if (ar_frame_ != nullptr && ar_session_ != nullptr) {
            ArHitResultList* hit_result_list = nullptr;
            ArHitResultList_create(ar_session_, &hit_result_list);
            CHECK(hit_result_list);
            if (is_instant_placement_enabled_) {
                ArFrame_hitTestInstantPlacement(ar_session_, ar_frame_, x, y,
                                                kApproximateDistanceMeters,
                                                hit_result_list);
            } else {
                ArFrame_hitTest(ar_session_, ar_frame_, x, y, hit_result_list);
            }

            int32_t hit_result_list_size = 0;
            ArHitResultList_getSize(ar_session_, hit_result_list,
                                    &hit_result_list_size);

            // The hitTest method sorts the resulting list by distance from the camera,
            // increasing.  The first hit result will usually be the most relevant when
            // responding to user input.

            ArHitResult* ar_hit_result = nullptr;
            for (int32_t i = 0; i < hit_result_list_size; ++i) {
                ArHitResult* ar_hit = nullptr;
                ArHitResult_create(ar_session_, &ar_hit);
                ArHitResultList_getItem(ar_session_, hit_result_list, i, ar_hit);

                if (ar_hit == nullptr) {
                    LOGE("HelloArApplication::OnTouched ArHitResultList_getItem error");
                    return;
                }

                ArTrackable* ar_trackable = nullptr;
                ArHitResult_acquireTrackable(ar_session_, ar_hit, &ar_trackable);
                ArTrackableType ar_trackable_type = AR_TRACKABLE_NOT_VALID;
                ArTrackable_getType(ar_session_, ar_trackable, &ar_trackable_type);
                // Creates an anchor if a plane or an oriented point was hit.
                if (AR_TRACKABLE_PLANE == ar_trackable_type) {
                    ArPose* hit_pose = nullptr;
                    ArPose_create(ar_session_, nullptr, &hit_pose);
                    ArHitResult_getHitPose(ar_session_, ar_hit, hit_pose);
                    int32_t in_polygon = 0;
                    ArPlane* ar_plane = ArAsPlane(ar_trackable);
                    ArPlane_isPoseInPolygon(ar_session_, ar_plane, hit_pose, &in_polygon);

                    // Use hit pose and camera pose to check if hittest is from the
                    // back of the plane, if it is, no need to create the anchor.
                    ArPose* camera_pose = nullptr;
                    ArPose_create(ar_session_, nullptr, &camera_pose);
                    ArCamera* ar_camera;
                    ArFrame_acquireCamera(ar_session_, ar_frame_, &ar_camera);
                    ArCamera_getPose(ar_session_, ar_camera, camera_pose);
                    ArCamera_release(ar_camera);
                    float normal_distance_to_plane = util::CalculateDistanceToPlane(
                            *ar_session_, *hit_pose, *camera_pose);

                    ArPose_destroy(hit_pose);
                    ArPose_destroy(camera_pose);

                    if (!in_polygon || normal_distance_to_plane < 0) {
                        continue;
                    }

                    ar_hit_result = ar_hit;
                    break;
                } else if (AR_TRACKABLE_POINT == ar_trackable_type) {
                    ArPoint* ar_point = ArAsPoint(ar_trackable);
                    ArPointOrientationMode mode;
                    ArPoint_getOrientationMode(ar_session_, ar_point, &mode);
                    if (AR_POINT_ORIENTATION_ESTIMATED_SURFACE_NORMAL == mode) {
                        ar_hit_result = ar_hit;
                        break;
                    }
                } else if (AR_TRACKABLE_INSTANT_PLACEMENT_POINT == ar_trackable_type) {
                    ar_hit_result = ar_hit;
                } else if (AR_TRACKABLE_DEPTH_POINT == ar_trackable_type) {
                    // ArDepthPoints are only returned if ArConfig_setDepthMode() is called
                    // with AR_DEPTH_MODE_AUTOMATIC.
                    ar_hit_result = ar_hit;
                }
            }

            if (ar_hit_result) {
                // Note that the application is responsible for releasing the anchor
                // pointer after using it. Call ArAnchor_release(anchor) to release.
                ArAnchor* anchor = nullptr;
                if (ArHitResult_acquireNewAnchor(ar_session_, ar_hit_result, &anchor) !=
                    AR_SUCCESS) {
                    LOGE(
                            "HelloArApplication::OnTouched ArHitResult_acquireNewAnchor error");
                    return;
                }

                ArTrackingState tracking_state = AR_TRACKING_STATE_STOPPED;
                ArAnchor_getTrackingState(ar_session_, anchor, &tracking_state);
                if (tracking_state != AR_TRACKING_STATE_TRACKING) {
                    ArAnchor_release(anchor);
                    return;
                }

                if (anchors_.size() >= kMaxNumberOfAndroidsToRender) {
                    ArAnchor_release(anchors_[0].anchor);
                    ArTrackable_release(anchors_[0].trackable);
                    anchors_.erase(anchors_.begin());
                }

                ArTrackable* ar_trackable = nullptr;
                ArHitResult_acquireTrackable(ar_session_, ar_hit_result, &ar_trackable);
                // Assign a color to the object for rendering based on the trackable type
                // this anchor attached to. For AR_TRACKABLE_POINT, it's blue color, and
                // for AR_TRACKABLE_PLANE, it's green color.
                ColoredAnchor colored_anchor;
                colored_anchor.anchor = anchor;
                colored_anchor.trackable = ar_trackable;

                UpdateAnchorColor(&colored_anchor);
                anchors_.push_back(colored_anchor);

                ArHitResult_destroy(ar_hit_result);
                ar_hit_result = nullptr;

                ArHitResultList_destroy(hit_result_list);
                hit_result_list = nullptr;
            }
        }
    }

    void HelloArApplication::UpdateAnchorColor(ColoredAnchor* colored_anchor) {
        if (colored_anchor->trackable == nullptr) {
            // 기본 흰색 설정
            SetColor(255.0f, 255.0f, 255.0f, 255.0f, colored_anchor->color);
            return;
        }
        ArTrackable* ar_trackable = colored_anchor->trackable;
        float* color = colored_anchor->color;

        ArTrackableType ar_trackable_type;
        ArTrackable_getType(ar_session_, ar_trackable, &ar_trackable_type);

        if (ar_trackable_type == AR_TRACKABLE_POINT) {
            SetColor(66.0f, 133.0f, 244.0f, 255.0f, color);
            return;
        }

        if (ar_trackable_type == AR_TRACKABLE_PLANE) {
            SetColor(139.0f, 195.0f, 74.0f, 255.0f, color);
            return;
        }

        if (ar_trackable_type == AR_TRACKABLE_DEPTH_POINT) {
            SetColor(199.0f, 8.0f, 65.0f, 255.0f, color);
            return;
        }

        if (ar_trackable_type == AR_TRACKABLE_INSTANT_PLACEMENT_POINT) {
            ArInstantPlacementPoint* ar_instant_placement_point =
                    ArAsInstantPlacementPoint(ar_trackable);
            ArInstantPlacementPointTrackingMethod tracking_method;
            ArInstantPlacementPoint_getTrackingMethod(
                    ar_session_, ar_instant_placement_point, &tracking_method);
            if (tracking_method ==
                AR_INSTANT_PLACEMENT_POINT_TRACKING_METHOD_FULL_TRACKING) {
                SetColor(255.0f, 255.0f, 137.0f, 255.0f, color);
                return;
            } else if (
                    tracking_method ==
                    AR_INSTANT_PLACEMENT_POINT_TRACKING_METHOD_SCREENSPACE_WITH_APPROXIMATE_DISTANCE) {  // NOLINT
                SetColor(255.0f, 255.0f, 255.0f, 255.0f, color);
                return;
            }
        }


        // Fallback color
        SetColor(0.0f, 0.0f, 0.0f, 0.0f, color);
    }

// This method returns a transformation matrix that when applied to screen space
// uvs makes them match correctly with the quad texture coords used to render
// the camera feed. It takes into account device orientation.
    glm::mat3 HelloArApplication::GetTextureTransformMatrix(
            const ArSession* session, const ArFrame* frame) {
        float frameTransform[6];
        float uvTransform[9];
        // XY pairs of coordinates in NDC space that constitute the origin and points
        // along the two principal axes.
        const float ndcBasis[6] = {0, 0, 1, 0, 0, 1};
        ArFrame_transformCoordinates2d(
                session, frame, AR_COORDINATES_2D_OPENGL_NORMALIZED_DEVICE_COORDINATES, 3,
                ndcBasis, AR_COORDINATES_2D_TEXTURE_NORMALIZED, frameTransform);

        // Convert the transformed points into an affine transform and transpose it.
        float ndcOriginX = frameTransform[0];
        float ndcOriginY = frameTransform[1];
        uvTransform[0] = frameTransform[2] - ndcOriginX;
        uvTransform[1] = frameTransform[3] - ndcOriginY;
        uvTransform[2] = 0;
        uvTransform[3] = frameTransform[4] - ndcOriginX;
        uvTransform[4] = frameTransform[5] - ndcOriginY;
        uvTransform[5] = 0;
        uvTransform[6] = ndcOriginX;
        uvTransform[7] = ndcOriginY;
        uvTransform[8] = 1;

        return glm::make_mat3(uvTransform);
    }
}  // namespace hello_ar