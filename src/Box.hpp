#pragma once

#include <Geode/Geode.hpp>

struct Box {
    cocos2d::CCPoint center, unitX, unitY;

    static Box fromOBB2D(const OBB2D& obb);
};

std::vector<cocos2d::CCPoint> calculateMinkowskiDifference(const Box& box0, const Box& box1);