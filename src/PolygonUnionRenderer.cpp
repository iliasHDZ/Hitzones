#include "PolygonUnionRenderer.hpp"

using namespace geode::prelude;

static float fullscreenQuad[] = {
    0.0f, 0.0f,
    0.0f, 1.0f,
    1.0f, 0.0f,
    1.0f, 1.0f,

    0.0f, 0.0f,
    0.0f, 1.0f,
    1.0f, 0.0f,
    1.0f, 1.0f,
};

PolygonUnionRenderer::~PolygonUnionRenderer() {
    if (vertexBuffer)
        glDeleteBuffers(1, &vertexBuffer);
    if (framebufferTexture)
        framebufferTexture->release();
    glDeleteBuffers(1, &fullscreenQuadBuffer);
}

bool PolygonUnionRenderer::init() {
    setShaderProgram(CCShaderCache::sharedShaderCache()->programForKey(kCCShader_PositionColor));
    fullscreenShader = CCShaderCache::sharedShaderCache()->programForKey(kCCShader_PositionTexture);

    ensureCapacity(256);

    glGenBuffers(1, &fullscreenQuadBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, fullscreenQuadBuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(fullscreenQuad), fullscreenQuad, GL_STATIC_DRAW);

    return true;
}

void PolygonUnionRenderer::draw() {
    if (!prepareFramebuffer())
        return;

    CC_NODE_DRAW_SETUP();
    getShaderProgram()->setUniformsForBuiltins();

    kmMat4 matrixP;
	kmMat4 matrixMV;
	kmMat4 matrixMVP;
	
	kmGLGetMatrix(KM_GL_PROJECTION, &matrixP);
	kmGLGetMatrix(KM_GL_MODELVIEW, &matrixMV);
	
	kmMat4Multiply(&matrixMVP, &matrixP, &matrixMV);
    
    getShaderProgram()->setUniformLocationWithMatrix4fv(fullscreenShader->getUniformLocationForName(kCCUniformPMatrix_s), matrixP.mat, 1);
    getShaderProgram()->setUniformLocationWithMatrix4fv(fullscreenShader->getUniformLocationForName(kCCUniformMVMatrix_s), matrixMV.mat, 1);
    getShaderProgram()->setUniformLocationWithMatrix4fv(fullscreenShader->getUniformLocationForName(kCCUniformMVPMatrix_s), matrixMVP.mat, 1);

    glDisable(GL_BLEND);
    i32 prevFramebuffer;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFramebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);

    if (dirty) {
        ensureCapacity(verticies.size());
        glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
        glBufferSubData(GL_ARRAY_BUFFER, 0, verticies.size() * sizeof(Vertex), verticies.data());
        dirty = false;
    }

    //// DRAWING THE POLYGONS ////

    ccGLEnableVertexAttribs(kCCVertexAttribFlag_Position | kCCVertexAttribFlag_Color);
    glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);

    glVertexAttribPointer(kCCVertexAttrib_Position, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), 0);
    glVertexAttribPointer(kCCVertexAttrib_Color, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vertex), (void*)offsetof(Vertex, color));

    glDrawArrays(GL_TRIANGLES, 0, verticies.size());

    //// POST DRAWING ////

    glEnable(GL_BLEND);
    glBindFramebuffer(GL_FRAMEBUFFER, prevFramebuffer);
    
    //// PREPARING FULLSCREEN DRAW ////

    float matrix[] = {
         2,  0, 0, 0,
         0,  2, 0, 0,
         0,  0, 0, 0,
        -1, -1, 0, 1
    };

    fullscreenShader->use();
    fullscreenShader->setUniformsForBuiltins();
    fullscreenShader->setUniformLocationWithMatrix4fv(fullscreenShader->getUniformLocationForName(kCCUniformMVPMatrix_s), matrix, 1);

    ccGLBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    ccGLBindTexture2D(framebufferTexture->getName());
    
    //// FULLSCREEN DRAW ////

    ccGLEnableVertexAttribs(kCCVertexAttribFlag_Position | kCCVertexAttribFlag_TexCoords);
    glBindBuffer(GL_ARRAY_BUFFER, fullscreenQuadBuffer);

    glVertexAttribPointer(kCCVertexAttrib_Position, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 2, 0);
    glVertexAttribPointer(kCCVertexAttrib_TexCoords, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 2, (void*)(sizeof(float) * 8));

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void PolygonUnionRenderer::ensureCapacity(u32 capacity) {
    if (capacity <= vertexBufferCapacity)
        return;

    u32 newCapacity = 1;
    while (newCapacity < capacity)
        newCapacity *= 2;
    capacity = newCapacity;
    
    vertexBufferCapacity = capacity;

    if (!vertexBuffer)
        glGenBuffers(1, &vertexBuffer);

    glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex) * capacity, NULL, GL_STREAM_DRAW);

    dirty = true;
}

void PolygonUnionRenderer::addConvexPolygon(std::span<const CCPoint> points, const ccColor4F& colorFloat) {
    if (points.size() < 3)
        return;

    CCPoint anchor = points[0];

    auto color = ccc4BFromccc4F(colorFloat);

    for (int i = 1; i < points.size() - 1; i++) {
        verticies.push_back({anchor, color});
        verticies.push_back({points[i], color});
        verticies.push_back({points[i + 1], color});
    }

    dirty = true;
}

void PolygonUnionRenderer::addInsetConvexPolygon(
    std::span<const CCPoint> points,
    const ccColor4F& color,
    float insetAmount
) {
    std::vector<CCPoint> insetPoints;
    insetPoints.reserve(points.size());

    for (int i = 0; i < points.size(); i++) {
        const auto& point = points[i];

        const auto& prevPoint = points[i <= 0 ? (points.size() - 1) : (i - 1)];
        const auto& nextPoint = points[i >= points.size() - 1 ? 0 : (i + 1)];

        auto prevVec = prevPoint - point;
        auto nextVec = nextPoint - point;

        float angle = CC_RADIANS_TO_DEGREES(ccpAngle(prevVec, nextVec));

        float inset = (insetAmount / cosf(CC_DEGREES_TO_RADIANS((180.0f - angle) / 2.0f)));

        auto insetPoint = point + (prevVec.normalize() + nextVec.normalize()).normalize() * inset;

        insetPoints.push_back(insetPoint);
    }

    addConvexPolygon(insetPoints, color);
}

bool PolygonUnionRenderer::prepareFramebuffer() {
    CCSize screenSize = CCEGLView::get()->getFrameSize();

    if (screenSize == CCSize(0, 0)) {
        lastScreenSize = screenSize;
        return false;
    }

    if (screenSize == lastScreenSize)
        return true;

    lastScreenSize = screenSize;

    if (framebufferTexture) {
        glDeleteFramebuffers(1, &fbo);
        framebufferTexture->release();
        framebufferTexture = nullptr;
    }

    framebufferTexture = new CCTexture2D;
    framebufferTexture->initWithData(nullptr, cocos2d::kCCTexture2DPixelFormat_RGBA8888, screenSize.width, screenSize.height, screenSize);
    framebufferTexture->retain();

    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, framebufferTexture->getName(), 0);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    return true;
}