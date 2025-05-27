#pragma once

#include "util.h"
#include <vector>
#include <map>

class AStarPathfinder {
public:
    void LoadPoseGraph(const std::string& path, int floor);
    std::vector<Point> astar(const Point& start, const Point& goal, int floor, const Point& pose_pos);

private:
    std::map<int, std::vector<Point>> pose_graph_by_floor_;

    std::map<int, std::vector<std::vector<std::pair<int, float>>>> adjacency_list_by_floor_;

    int current_floor_ = 0;
    float threshold = 1.5f;

    int FindClosestNode(const Point& coord, int floor);
};

float heuristic(const Point& a, const Point& b);