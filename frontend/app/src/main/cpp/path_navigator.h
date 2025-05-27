#pragma once

#include <queue>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <string>
#include <cmath>
#include <android/log.h>
#include <glm/glm.hpp>

#include "util.h"
#include "direction_helper.h"
#include "astar_pathfinding.h"
#include "java_bridge.h"

class PathNavigator {
public:
    PathNavigator();

    void SetGoals(const std::vector<Point>& goals);
    void TryGeneratePathIfNeeded(const Point& camera_pos, const Point& pose_pos);
    bool UpdateNavigation(const Point& cam_pos, const float* pose_raw, DirectionHelper& direction_helper, const Point& pose_pos);
    bool IsReadyToRender() const;
    void SetReadyToRenderFalse();
    const std::vector<Point>& GetPath() const;
    int GetCurrentPathIndex() const;
    void Reset();
    bool IsGoalSet() const { return goal_set_; }

    bool getarrival();
    void SetCurrentFloor(int current_floor);
    void ChangeStatus();
    bool GetStatusFlag();

    AStarPathfinder astar_pathfinding_;

private:
    std::queue<Point> goal_queue_;
    std::queue<Point> start_queue_;

    std::vector<Point> path_;
    bool path_generated_ = false;
    bool path_ready_to_render_ = false;
    int current_path_index_ = 0;

    bool goal_set_ = false;
    bool arrival_ = false;

    std::mutex m_status_flag;
    bool status_flag = true;

    std::unordered_set<int> notified_turn_indices_;

    int current_floor_;
};
