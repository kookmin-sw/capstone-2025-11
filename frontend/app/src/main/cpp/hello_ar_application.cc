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
     HelloArApplication::HelloArApplication(AAssetManager* asset_manager, std::string& external_path)
         : pose_graph(external_path, "brief_pattern.yml", "brief_k10L6.bin", false, 0.2, 640, 480),
         asset_manager_(asset_manager), location_pin_anchor_{nullptr, nullptr} {
     
         LOGI("external_path: %s", external_path.c_str());
         pose_graph.loadVocabulary(asset_manager_);
     }
 
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
 
             ArConfig* ar_config = nullptr;
             ArConfig_create(ar_session_, &ar_config);
             ArConfig_setDepthMode(ar_session_, ar_config, AR_DEPTH_MODE_AUTOMATIC);
             CHECK(ArSession_configure(ar_session_, ar_config) == AR_SUCCESS);
             ArConfig_destroy(ar_config);
 
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
         background_renderer_.InitializeGlContent(asset_manager_, depth_texture_.GetTextureId());
         location_pin_renderer_.InitializeGlContent(asset_manager_, "models/location_pin.obj", "models/location_pin.png");
         plane_renderer_.InitializeGlContent(asset_manager_);
         car_arrow_renderer_.InitializeGlContent(asset_manager_, "models/carArrow.obj", "models/carArrow.png");
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
 
         ArCamera* ar_camera = nullptr;
         ArFrame_acquireCamera(ar_session_, ar_frame_, &ar_camera);
 
         // 카메라 트래킹 상태 확인
         ArTrackingState camera_tracking_state;
         ArCamera_getTrackingState(ar_session_, ar_camera, &camera_tracking_state);
 
         if (camera_tracking_state != AR_TRACKING_STATE_TRACKING) {
             LOGI("⚠️ 카메라 트래킹 안됨 - 앵커 및 경로 생성 생략");
             return;
         }
 
         // camera pose raw 가져옴
         ArPose* camera_pose = nullptr;
         ArPose_create(ar_session_, nullptr, &camera_pose);
         ArCamera_getPose(ar_session_, ar_camera, camera_pose);
 
         float pose_raw[7];
         ArPose_getPoseRaw(ar_session_, camera_pose, pose_raw);
 
         float matrix[16];
         ArPose_getMatrix(ar_session_, camera_pose, matrix);
 
         glm::vec3 cam_pos_vec3 = PoseHelper::GetCameraPosition(pose_raw);
         Point cam_pos{cam_pos_vec3.x, cam_pos_vec3.z};
 
         // PathNavigator로 경로 생성 시도
         path_navigator_.TryGeneratePathIfNeeded(cam_pos);
 
         // 경로 따라가기
         path_navigator_.UpdateNavigation(cam_pos, matrix, direction_helper_);
 
         if(this->intrinsic_param.fx == 0.0){
             ArCameraIntrinsics* intrinsics = nullptr;
             ArCameraIntrinsics_create(ar_session_, &intrinsics);
 
             ArCamera_getImageIntrinsics(ar_session_, ar_camera, intrinsics);
 
             float fx, fy, cx, cy;
             ArCameraIntrinsics_getFocalLength(ar_session_, intrinsics, &fx, &fy);
             ArCameraIntrinsics_getPrincipalPoint(ar_session_, intrinsics, &cx, &cy);
             LOGI("focal length : %f, %f | principal point : %f, %f", fx, fy, cx, cy);
 
             this->intrinsic_param.update_parameter(fx, fy, cx, cy);
 
             ArCameraIntrinsics_destroy(intrinsics);
         }
 
         JNIEnv* env = GetJniEnv(); 
 
         if (env) {
             // yaw 정보 JNI로 전달
             float camera_yaw = direction_helper_.GetLastCameraYaw();
             float path_yaw = direction_helper_.GetLastPathYaw();
         
             jclass clazz = env->FindClass("com/capstone/whereigo/HelloArFragment");
             jmethodID method = env->GetStaticMethodID(clazz, "updateYawFromNative", "(FF)V");
         
             if (clazz && method) {
                 env->CallStaticVoidMethod(clazz, method, camera_yaw, path_yaw);
             }
         }
 
         glm::mat4 view_mat;
         glm::mat4 projection_mat;
         ArCamera_getViewMatrix(ar_session_, ar_camera, glm::value_ptr(view_mat));
         ArCamera_getProjectionMatrix(ar_session_, ar_camera,
                 /*near=*/0.1f, /*far=*/100.f,
                                      glm::value_ptr(projection_mat));
 
         Eigen::Vector3d _vio_T_w_i;
         Eigen::Matrix3d _vio_R_w_i;
 
         _vio_T_w_i = Eigen::Vector3d(pose_raw[4], pose_raw[5], pose_raw[6]);
         Eigen::Quaterniond q(pose_raw[3], pose_raw[0], pose_raw[1], pose_raw[2]);  // w, x, y, z
         _vio_R_w_i = q.toRotationMatrix();
 
         ArImage* depth_image = nullptr;
         ArImage* image = nullptr;
         if(ArFrame_acquireCameraImage(ar_session_, ar_frame_, &image) == AR_SUCCESS) {
             auto depth_status = ArFrame_acquireDepthImage16Bits(ar_session_, ar_frame_, &depth_image);
             if (depth_status == AR_SUCCESS) {
                 LOGI("status : %d", depth_status);
                 const uint8_t *image_data = nullptr;
                 const uint8_t *depth_data = nullptr;
 
                 int image_width, image_height, image_pixel_stride, image_data_length, image_row_stride;
                 int depth_width, depth_height, depth_pixel_stride, depth_data_length, depth_row_stride;
 
                 ArImage_getWidth(ar_session_, image, &image_width);
                 ArImage_getHeight(ar_session_, image, &image_height);
                 ArImage_getPlaneRowStride(ar_session_, image, 0, &image_row_stride);
                 ArImage_getPlanePixelStride(ar_session_, image, 0, &image_pixel_stride);
                 ArImage_getPlaneData(ar_session_, image, 0, &image_data, &image_data_length);
 
                 ArImage_getWidth(ar_session_, depth_image, &depth_width);
                 ArImage_getHeight(ar_session_, depth_image, &depth_height);
                 ArImage_getPlaneRowStride(ar_session_, depth_image, 0, &depth_row_stride);
                 ArImage_getPlanePixelStride(ar_session_, depth_image, 0, &depth_pixel_stride);
                 ArImage_getPlaneData(ar_session_, depth_image, 0, &depth_data, &depth_data_length);
 
                 cv::Mat image_mat = cv::Mat(image_height, image_width, CV_8UC1, const_cast<uint8_t *>(image_data), image_row_stride);
 
                 depth_row_stride /= 2;
                 cv::Mat depth_mat = cv::Mat(depth_height, depth_width, CV_16UC1, const_cast<uint8_t *>(depth_data), depth_row_stride);
                 cv::resize(depth_mat, depth_mat, image_mat.size());
 
                 KeyFramePtr keyframe = std::make_shared<KeyFrame>(0, _vio_T_w_i, _vio_R_w_i, image_mat, 0);
                 keyframe->computeBRIEFPoint(asset_manager_, intrinsic_param, depth_mat);
                 pose_graph.addKeyFrameBuf(keyframe);
                 if(0)
                 {
                    flag = false;
                    pose_graph.InitialPose(keyframe);
                 }
             }
         }
 
         // 6. Pose 객체 해제
         ArImage_release(image);
         ArImage_release(depth_image);
         ArPose_destroy(camera_pose);
 
         background_renderer_.Draw(ar_session_, ar_frame_,
                                   depthColorVisualizationEnabled);
 
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
 
         if (path_navigator_.IsReadyToRender()) {
 
             if (location_pin_anchor_.anchor != nullptr) ArAnchor_release(location_pin_anchor_.anchor);
             if (location_pin_anchor_.trackable != nullptr) ArTrackable_release(location_pin_anchor_.trackable);
 
             for (auto& anchor : carArrow_anchors_) {
                 if (anchor.anchor != nullptr) ArAnchor_release(anchor.anchor);
                 if (anchor.trackable != nullptr) ArTrackable_release(anchor.trackable);
             }
             carArrow_anchors_.clear();
 
             const auto& path = path_navigator_.GetPath();
             int current_index = path_navigator_.GetCurrentPathIndex();
             int path_size = path.size();
 
             for (int i = 0; i < 5; ++i) {
                 int idx = current_index + i;
                 if (idx >= path_size) break;
 
                 const Point& p = path[idx];
 
                 float anchor_pose[7] = {0};
                 anchor_pose[4] = p.x;
                 anchor_pose[5] = plane_y_;
                 anchor_pose[6] = p.z;
 
                 ArPose* pose = nullptr;
                 ArPose_create(ar_session_, anchor_pose, &pose);
 
                 ArAnchor* anchor = nullptr;
                 if (ArSession_acquireNewAnchor(ar_session_, pose, &anchor) == AR_SUCCESS) {
                     ColoredAnchor car_anchor;
                     car_anchor.anchor = anchor;
                     car_anchor.trackable = nullptr;
                     SetColor(1.0f, 1.0f, 1.0f, 1.0f, car_anchor.color);
                     carArrow_anchors_.push_back(car_anchor);
                 }
                 ArPose_destroy(pose);
             }
             path_navigator_.SetReadyToRenderFalse();
 
 
             const auto& p = path.back();
             float anchor_pose[7] = {0};
             anchor_pose[4] = p.x;
             anchor_pose[5] = plane_y_ + 2.3f;
             anchor_pose[6] = p.z;
 
             ArPose* pose = nullptr;
             ArPose_create(ar_session_, anchor_pose, &pose);
 
             ArAnchor* anchor = nullptr;
             if (ArSession_acquireNewAnchor(ar_session_, pose, &anchor) == AR_SUCCESS) {
                 location_pin_anchor_.anchor = anchor;
                 location_pin_anchor_.trackable = nullptr;
                 SetColor(255, 0, 0, 255, location_pin_anchor_.color);
             }
             ArPose_destroy(pose);
 
             path_navigator_.SetReadyToRenderFalse();
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
 
         for (int i = 0; i < carArrow_anchors_.size(); ++i) {
             if (i >= path_navigator_.GetPath().size()) break;
 
             glm::mat4 model_mat(1.0f);
 
             // ⭐ Anchor로부터 변환행렬 가져오기 ⭐
             util::GetTransformMatrixFromAnchor(*carArrow_anchors_[i].anchor, ar_session_, &model_mat);
 
             // 추가로 방향 회전은 여기서 적용
             const auto& path = path_navigator_.GetPath();
             int current_index = path_navigator_.GetCurrentPathIndex() + i;
             if (current_index >= path.size()) continue;
 
             const Point& from = path[current_index];
             Point to = (current_index + 1 < path.size()) ? path[current_index + 1] : from;
 
             glm::vec3 direction(to.x - from.x, 0.0f, to.z - from.z);
             float length = glm::length(direction);
             if (length < 0.01f) continue;
             direction = glm::normalize(direction);
 
             float angle = std::atan2(direction.x, direction.z) - glm::pi<float>();
 
             glm::vec3 position(from.x, plane_y_, from.z);
 
             model_mat = glm::translate(glm::mat4(1.0f), position);
             glm::mat4 rotation_mat = glm::rotate(glm::mat4(1.0f), angle, glm::vec3(0, 1, 0));
             glm::mat4 scale_mat = glm::scale(glm::mat4(1.0f), glm::vec3(0.05f));
 
             model_mat = model_mat * rotation_mat * scale_mat;
 
             // 렌더링
             car_arrow_renderer_.Draw(projection_mat, view_mat, model_mat, color_correction, carArrow_anchors_[i].color);
         }
 
 
         if (location_pin_anchor_.anchor != nullptr) {
             const auto& path = path_navigator_.GetPath();
             int path_size = static_cast<int>(path.size());
             int current_index = path_navigator_.GetCurrentPathIndex();
 
             if (current_index >= path_size - 5 && !path_navigator_.arrival_) {
                 glm::mat4 model_mat(1.0f);
                 if (location_pin_anchor_.trackable != nullptr) {
                     UpdateAnchorColor(&location_pin_anchor_);
                 }
                 util::GetTransformMatrixFromAnchor(*location_pin_anchor_.anchor, ar_session_, &model_mat);
                 location_pin_renderer_.Draw(projection_mat, view_mat, model_mat, color_correction, location_pin_anchor_.color);
             }
         }
 
         ArCamera_release(ar_camera);
     }
 
     void HelloArApplication::SavePoseGraph() {
         pose_graph.command();
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
 }  // namespace hello_ar