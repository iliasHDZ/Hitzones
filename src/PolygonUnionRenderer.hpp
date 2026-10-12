#pragma once

#include "common.hpp"

class PolygonUnionRenderer : public cocos2d::CCNode {
public:
    ~PolygonUnionRenderer();

    bool init() override;

    void draw() override;

    inline void clear() {
        verticies.clear();
        dirty = true;
    }

    CREATE_FUNC(PolygonUnionRenderer);

    void addConvexPolygon(std::span<const cocos2d::CCPoint> points, const cocos2d::ccColor4F& color);

    void addInsetConvexPolygon(
        std::span<const cocos2d::CCPoint> points,
        const cocos2d::ccColor4F& color,
        float insetAmount
    );

private:
    struct Vertex {
        cocos2d::CCPoint position;
        cocos2d::ccColor4B color;
    };
    void ensureCapacity(u32 capacity);

    bool prepareFramebuffer();

private:
    bool dirty = false;

    //// POLYGON VERTICIES ////
    u32 vertexBuffer = 0;
    std::vector<Vertex> verticies;
    u32 vertexBufferCapacity = 0;

    //// FRAMEBUFFER ////
    cocos2d::CCTexture2D* framebufferTexture = nullptr;
    unsigned int fbo = 0;
    cocos2d::CCSize lastScreenSize = {0, 0};
    u32 fullscreenQuadBuffer = 0;
    cocos2d::CCGLProgram* fullscreenShader = nullptr;
};