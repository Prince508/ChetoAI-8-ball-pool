#include "physics.h"
#include <cmath>

// Helper: Calculate distance between two points
float Physics::distance(const cv::Point2f& p1, const cv::Point2f& p2) {
    return static_cast<float>(std::sqrt(std::pow(p2.x - p1.x, 2) + std::pow(p2.y - p1.y, 2)));
}

// Helper: Normalize a vector
cv::Point2f Physics::normalize(const cv::Point2f& vec) {
    float mag = std::sqrt(vec.x * vec.x + vec.y * vec.y);
    if (mag == 0) return vec;
    return cv::Point2f(vec.x / mag, vec.y / mag);
}

// Helper: Reflect a vector over a normal (for future cushion shots)
cv::Point2f Physics::reflectVector(const cv::Point2f& incident, const cv::Point2f& normal) {
    float dot = incident.x * normal.x + incident.y * normal.y;
    return cv::Point2f(incident.x - 2 * dot * normal.x, incident.y - 2 * dot * normal.y);
}

// Helper: Check if a line intersects a rectangle (table bounds)
bool Physics::lineIntersectsRect(const cv::Point2f& start, const cv::Point2f& end, const cv::Rect& rect, cv::Point2f& intersection) {
    std::vector<cv::Point2f> sides[4] = {
        { {cv::Point2f(static_cast<float>(rect.x), static_cast<float>(rect.y)), cv::Point2f(static_cast<float>(rect.x + rect.width), static_cast<float>(rect.y))} }, // Top
        { {cv::Point2f(static_cast<float>(rect.x), static_cast<float>(rect.y + rect.height)), cv::Point2f(static_cast<float>(rect.x + rect.width), static_cast<float>(rect.y + rect.height))} }, // Bottom
        { {cv::Point2f(static_cast<float>(rect.x), static_cast<float>(rect.y)), cv::Point2f(static_cast<float>(rect.x), static_cast<float>(rect.y + rect.height))} }, // Left
        { {cv::Point2f(static_cast<float>(rect.x + rect.width), static_cast<float>(rect.y)), cv::Point2f(static_cast<float>(rect.x + rect.width), static_cast<float>(rect.y + rect.height))} }  // Right
    };

    for (const auto& side : sides) {
        float denom = (end.x - start.x) * (side[1].y - side[0].y) - (end.y - start.y) * (side[1].x - side[0].x);
        if (denom == 0) continue;

        float t = ((start.y - side[0].y) * (side[1].x - side[0].x) - (start.x - side[0].x) * (side[1].y - side[0].y)) / denom;
        float u = ((start.y - side[0].y) * (end.x - start.x) - (start.x - side[0].x) * (end.y - start.y)) / denom;

        if (t >= 0 && t <= 1 && u >= 0 && u <= 1) {
            intersection = cv::Point2f(start.x + t * (end.x - start.x), start.y + t * (end.y - start.y));
            return true;
        }
    }
    return false;
}

// Compute ghost ball position (where cue ball should hit target ball)
cv::Point2f Physics::computeGhostBall(const Ball& cue, const Ball& target) {
    cv::Point2f direction = normalize(target.center - cue.center);
    float combinedRadius = cue.radius + target.radius;
    if (combinedRadius == 0) return target.center;
    return target.center - direction * combinedRadius;
}

// Main function: Predict shot path and extend to boundary or pocket
std::vector<LineSegment> calculateGuideline(const Ball& cueBall, const Ball& targetBall, const Table& table) {
    std::vector<LineSegment> guideline;

    // Compute the direction vector from the cue ball to the target ball
    cv::Point2f direction = targetBall.center - cueBall.center;
    cv::Point2f normalizedDirection = Physics::normalize(direction);

    // Extend the line from the cue ball to the target ball
    LineSegment cueToTarget;
    cueToTarget.start = cueBall.center;
    cueToTarget.end = targetBall.center;
    guideline.push_back(cueToTarget);

    // Compute the ghost ball position
    cv::Point2f ghostBall = Physics::computeGhostBall(cueBall, targetBall);

    // Extend the line from the target ball to the ghost ball
    LineSegment targetToGhost;
    targetToGhost.start = targetBall.center;
    targetToGhost.end = ghostBall;
    guideline.push_back(targetToGhost);

    // Optionally, extend the line to the table boundary or pocket
    for (const auto& pocket : table.pockets) {
        LineSegment toPocket;
        toPocket.start = targetBall.center;
        toPocket.end = pocket;
        guideline.push_back(toPocket);
    }

    return guideline;
}

std::vector<LineSegment> Physics::predictShotPath(const Ball& cue, const Ball& target, const Table& table) {
    std::vector<LineSegment> segments;

    // Segment 1: Cue ball to ghost ball
    cv::Point2f ghostBall = computeGhostBall(cue, target);
    segments.push_back({ cue.center, ghostBall });

    // Segment 2: Target ball to pocket/boundary
    cv::Point2f direction = normalize(target.center - ghostBall);
    cv::Point2f extendedEnd = target.center + direction * 1000;
    cv::Point2f intersection;

    bool pocketHit = false;
    for (const auto& pocket : table.pockets) {
        float dist = distance(target.center, pocket);
        if (dist < 20.0f) {
            extendedEnd = pocket;
            pocketHit = true;
            break;
        }
    }

    if (!pocketHit && lineIntersectsRect(target.center, extendedEnd, table.bounds, intersection)) {
        extendedEnd = intersection;
    }

    segments.push_back({ target.center, extendedEnd });

    return segments;
}