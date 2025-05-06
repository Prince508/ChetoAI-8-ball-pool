#pragma once

#include <opencv2/opencv.hpp>
#include <vector>

// Enum for ball types
enum class BallType {
    Cue = 0,
    Target = 1,
    Other = 2
};

struct Ball {
    cv::Point2f center; // Center point (x, y) from YOLO detection
    float radius;       // Estimated radius (based on bounding box)
    BallType type;      // Ball type using enum
};

struct Table {
    cv::Rect bounds;    // Play_Area bounding box from YOLO
    std::vector<cv::Point2f> pockets; // Hole centers from YOLO
};

struct LineSegment {
    cv::Point2f start;
    cv::Point2f end;
};

class Physics {
public:
    // Predict the shot path from cue to target, extending to boundary or pocket
    static std::vector<LineSegment> predictShotPath(const Ball& cue, const Ball& target, const Table& table);

    // Compute ghost ball position for visualization
    static cv::Point2f computeGhostBall(const Ball& cue, const Ball& target);

    // Make normalize public to allow external access
    static cv::Point2f normalize(const cv::Point2f& vec);

private:
    // Helper functions adapted from repo
    static cv::Point2f reflectVector(const cv::Point2f& incident, const cv::Point2f& normal);
    static bool lineIntersectsRect(const cv::Point2f& start, const cv::Point2f& end, const cv::Rect& rect, cv::Point2f& intersection);
    static float distance(const cv::Point2f& p1, const cv::Point2f& p2);
};

// Declaration of calculateGuideline
std::vector<LineSegment> calculateGuideline(const Ball& cueBall, const Ball& targetBall, const Table& table);
