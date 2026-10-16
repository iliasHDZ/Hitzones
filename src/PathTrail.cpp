#include "PathTrail.hpp"

using namespace geode::prelude;

#define kPathLifetime 2.0f

void PathTrail::update(float dt) {
    for (auto& point : points)
        point.lifetime -= dt;

    while (points.size() > 0 && points[0].lifetime <= 0.0f)
        points.erase(points.begin());

    for (auto& input : inputs)
        input.lifetime -= dt;

    while (inputs.size() > 0 && inputs[0].lifetime <= 0.0f)
        inputs.erase(inputs.begin());
}

void PathTrail::addPoint(const CCPoint& point) {
    points.push_back({point, kPathLifetime});
}

void PathTrail::draw(CCDrawNode* drawNode) {
    if (points.size() < 2)
        return;

    for (int i = 0; i < points.size() - 1; i++) {
        std::array<CCPoint, 2> pointPair = { points[i].position, points[i + 1].position };
        drawNode->drawLines(pointPair.data(), 2, 1.0f, {0.0f, 1.0f, 0.0f, 1.0f});
    }

    for (const auto& input : inputs) {
        ccColor4F color = input.pressed
                            ? ccColor4F { 0.2, 0.2, 1, 1 }
                            : ccColor4F { 1, 0.6, 0, 1 };

		drawNode->drawCircle(input.position, 3.0f, color, 0.5f, {0, 0, 0, 1}, 8);
    }
}

void PathTrail::addInput(bool pressed) {
    if (points.size() == 0)
        return;

    CCPoint point = points[points.size() - 1].position;
    if (points.size() > 1)
        point = points[points.size() - 2].position;

    inputs.push_back({point, kPathLifetime, pressed});
}