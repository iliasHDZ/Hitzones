#pragma once

#include <Geode/Geode.hpp>

class PathTrail {
public:
    void update(float dt);

    void addPoint(const cocos2d::CCPoint& point);

    void draw(cocos2d::CCDrawNode* drawNode, const cocos2d::ccColor3B& color);

    inline void clear() {
        points.clear();
        inputs.clear();
    }

    void addInput(bool pressed);

private:
    struct PathPoint {
        cocos2d::CCPoint position;
        float lifetime;
    };

    struct InputPoint {
        cocos2d::CCPoint position;
        float lifetime;
        bool pressed;
    };

    std::vector<PathPoint> points;
    std::vector<InputPoint> inputs;
};