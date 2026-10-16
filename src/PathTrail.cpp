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

void PathTrail::draw(CCDrawNode* drawNode, const ccColor3B& color) {
    if (points.size() < 2)
        return;

    for (int i = 0; i < points.size() - 1; i++) {
        std::array<CCPoint, 2> pointPair = { points[i].position, points[i + 1].position };
        drawNode->drawLines(pointPair.data(), 2, 1.0f, ccc4FFromccc3B(color));
    }

    for (const auto& input : inputs) {
        auto color = input.pressed
                            ? ccColor3B { 252, 140, 3 }
                            : ccColor3B { 3, 157, 252 };

        auto colorFloat = ccc4FFromccc3B(color);

        if (input.pressed)
            drawNode->drawRect(CCRect {input.position - 2.0f, {4.0f, 4.0f}}, colorFloat, 0.0f, {0});
        else
		    drawNode->drawCircle(input.position, 2.0f, colorFloat, 0.0f, {0}, 8);
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