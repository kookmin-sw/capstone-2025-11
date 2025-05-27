#include "path_navigator.h"

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "PathNavigator", __VA_ARGS__)

namespace {
    constexpr float kDeviationThreshold = 3.0f;
    constexpr float kReachThreshold = 0.8f;
}

PathNavigator::PathNavigator() {
}

void PathNavigator::SetCurrentFloor(int current_floor){
    current_floor_ = current_floor;
    LOGI("currentFloor: %d", current_floor_);
}

void PathNavigator::SetGoals(const std::vector<Point>& goals) {
    while (!goal_queue_.empty()) goal_queue_.pop();
    while (!start_queue_.empty()) start_queue_.pop();

    start_queue_.push(Point{0.0f, 0.0f});

    for (size_t i = 0; i < goals.size(); ++i) {
        if (i % 2 == 0) {
            goal_queue_.push(goals[i]);
        } else {
            start_queue_.push(goals[i]);
        }
    }

    goal_set_ = !goal_queue_.empty();
    path_generated_ = false;

    LOGI("✅ Goal Queue 설정 완료. 총 %zu개 목표", goal_queue_.size());
    LOGI("✅ Start Queue 설정 완료. 총 %zu개 시작점", start_queue_.size());

    std::queue<Point> goal_debug = goal_queue_;
    int idx = 0;
    while (!goal_debug.empty()) {
        const Point& p = goal_debug.front();
        LOGI("🎯 Goal %d: x=%.2f, z=%.2f", idx++, p.x, p.z);
        goal_debug.pop();
    }

    std::queue<Point> start_debug = start_queue_;
    idx = 0;
    while (!start_debug.empty()) {
        const Point& p = start_debug.front();
        LOGI("🚩 Start %d: x=%.2f, z=%.2f", idx++, p.x, p.z);
        start_debug.pop();
    }
}

bool PathNavigator::GetStatusFlag(){
    return status_flag;
}


void PathNavigator::ChangeStatus() {
    while(!m_status_flag.try_lock());
    status_flag = !status_flag;
    LOGI("status_flag: %d", status_flag);
    m_status_flag.unlock();
}

void PathNavigator::TryGeneratePathIfNeeded(const Point& camera_pos, const Point& pose_pos) {
    if (!goal_set_ || path_generated_ || goal_queue_.empty() || !status_flag) return;

    Point current_start = start_queue_.front();
    Point current_goal = goal_queue_.front();

    path_ = astar_pathfinding_.astar(current_start + camera_pos + pose_pos, current_goal, current_floor_, pose_pos);

    if (!path_.empty()) {
        path_generated_ = true;
        path_ready_to_render_ = true;
        arrival_ = false;
        LOGI("🚀 경로 생성 완료. 다음 목표: x=%.2f, z=%.2f", current_goal.x, current_goal.z);

        for (size_t i = 0; i < path_.size(); ++i) {
            LOGI("경로 %zu: x=%.2f, z=%.2f", i, path_[i].x, path_[i].z);
        }

        JavaBridge::SpeakText("경로 안내를 시작합니다. 진동이 나는 방향을 찾아주세요.");
    } else {
        LOGI("❌ 경로 생성 실패");
    }
}

bool PathNavigator::getarrival() {
    return arrival_;
}

bool PathNavigator::UpdateNavigation(const Point& cam_pos, const float* matrix, DirectionHelper& direction_helper, const Point& pose_pos) {
    if (!goal_set_ || arrival_ || !status_flag || path_.empty()) return true;

    if (current_path_index_ > 0 && current_path_index_ < path_.size() - 2) {
        if (notified_turn_indices_.find(current_path_index_) == notified_turn_indices_.end()) {
            Point prev = path_[current_path_index_];
            Point current = path_[current_path_index_ + 1];
            Point next = path_[current_path_index_ + 2];

            float dx1 = current.x - prev.x;
            float dz1 = current.z - prev.z;
            float dx2 = next.x - current.x;
            float dz2 = next.z - current.z;

            float dot = dx1 * dx2 + dz1 * dz2;
            float mag1 = std::sqrt(dx1 * dx1 + dz1 * dz1);
            float mag2 = std::sqrt(dx2 * dx2 + dz2 * dz2);

            if (mag1 > 0.01f && mag2 > 0.01f) {
                float angle_cos = dot / (mag1 * mag2);
                if (angle_cos < 0.85f) {
                    JavaBridge::SpeakText("곧 방향 회전이 있습니다. 진동이 나는 방향을 찾아주세요.");
                    notified_turn_indices_.insert(current_path_index_);
                }
            }
        }
    }

    Point target = path_[current_path_index_];
    float dx = cam_pos.x - target.x;
    float dz = cam_pos.z - target.z;
    float distance = std::sqrt(dx * dx + dz * dz);

    if (distance > kDeviationThreshold) {
        LOGI("🚨 경로 이탈 감지됨. 재탐색 시작");
        JavaBridge::SpeakText("경로를 이탈하였습니다. 경로를 재탐색합니다.");

        Reset();
        path_generated_ = false;
        TryGeneratePathIfNeeded(cam_pos, pose_pos);
        return false;
    }

    direction_helper.Check(matrix, cam_pos, target);

    if (distance < kReachThreshold) {
        direction_helper.Reset();
        current_path_index_++;
        LOGI("✅ 경로 지점 %d 도달", current_path_index_);

        if (current_path_index_ >= path_.size()) {
            start_queue_.pop();
            goal_queue_.pop();
            Reset();

            if (!goal_queue_.empty()) {
                LOGI("➡️ 다음 목표로 이동");
                JavaBridge::SpeakText("다음 목표로 이동합니다.");
                ChangeStatus();
                JavaBridge::NotifyGoalStatus(0);
            } else {
                LOGI("목적지 도달 완료");
                if (!arrival_) {
                    JavaBridge::SpeakText("목적지에 도착하였습니다. 경로 안내를 종료합니다.");
                    arrival_ = true;
                }
                goal_set_ = false;

                JavaBridge::NotifyGoalStatus(1);
            }
            return true;
        }
    }

    return false;
}

bool PathNavigator::IsReadyToRender() const {
    return path_ready_to_render_;
}

void PathNavigator::SetReadyToRenderFalse() {
    path_ready_to_render_ = false;
}

const std::vector<Point>& PathNavigator::GetPath() const {
    return path_;
}

int PathNavigator::GetCurrentPathIndex() const {
    return current_path_index_;
}

void PathNavigator::Reset() {
    path_.clear();
    path_generated_ = false;
    path_ready_to_render_ = false;
    arrival_ = false;
    current_path_index_ = 0;
    notified_turn_indices_.clear();
}
