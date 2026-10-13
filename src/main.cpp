#include <Geode/Geode.hpp>
#include "Box.hpp"
#include "PolygonUnionRenderer.hpp"

using namespace geode::prelude;

#define kHazardColor 1, 0, 0
#define kSolidColor 0, 0, 1
#define kBorderWidth 1.0f

static constexpr ccColor4F solidBorderColor = {kSolidColor, 1};
static constexpr ccColor4F hazardBorderColor = {kHazardColor, 1};

static std::vector<CCPoint> generateRect(const CCRect& rect) {
	return {
		ccp(rect.getMinX(), rect.getMinY()),
		ccp(rect.getMinX(), rect.getMaxY()),
		ccp(rect.getMaxX(), rect.getMaxY()),
		ccp(rect.getMaxX(), rect.getMinY())
	};
}

#define kCircleSegments 48

static std::vector<CCPoint> generateCircle(const CCPoint& center, float radius) {
	std::vector<CCPoint> points;
	points.reserve(kCircleSegments);

	for (int i = 0; i < kCircleSegments; i++) {
		float angle = (float)i / (float)kCircleSegments * M_PI * 2;

		points.push_back(CCPoint(cos(angle), sin(angle)) * radius + center);
	}

	return points;
}

#define kBlockInset 0.3f

struct PathPoint {
	CCPoint point;
	float lifetime;
};

static std::vector<PathPoint> playerPath;

static bool hitzonesEnabled = false;
static bool hitzonesOnDeath = false;
static bool showTrailLine = false;

#define kPathLifetime 2.0f

static bool doIntersect(CCPoint p1, CCPoint q1, CCPoint p2, CCPoint q2, CCPoint& res) {
    double a1 = q1.y - p1.y;
    double b1 = p1.x - q1.x;
    double c1 = a1 * p1.x + b1 * p1.y;

    double a2 = q2.y - p2.y;
    double b2 = p2.x - q2.x;
    double c2 = a2 * p2.x + b2 * p2.y;

    double determinant = a1 * b2 - a2 * b1;

    if (determinant != 0) {
        res.x = (c1 * b2 - c2 * b1) / determinant;
        res.y = (a1 * c2 - a2 * c1) / determinant;
        return true;
    }

    return false;
}

#include <Geode/modify/PlayLayer.hpp>
class $modify(MyPlayLayer, PlayLayer) {
	struct Fields {
		PolygonUnionRenderer* renderer;
		CCDrawNode* drawNode;
	};

	bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
		if (!PlayLayer::init(level, useReplay, dontCreateObjects)) return false;
		
		auto node = PolygonUnionRenderer::create();
		node->setZOrder(5000);
		m_objectLayer->addChild(node);
		m_fields->renderer = node;

		auto node1 = CCDrawNode::create();
		node1->setZOrder(5001);
		m_objectLayer->addChild(node1);
		m_fields->drawNode = node1;

		playerPath.clear();

		hitzonesEnabled = Mod::get()->getSettingValue<bool>("enabled");
		hitzonesOnDeath = Mod::get()->getSettingValue<bool>("hitzones-on-death");
		showTrailLine = Mod::get()->getSettingValue<bool>("show-trail-line");

		return true;
	}

	void resetLevel() {
		PlayLayer::resetLevel();
		playerPath.clear();
	}

	void updateVisibility(float dt) {
		bool isEnabled = hitzonesEnabled || (hitzonesOnDeath && m_playerDied);

		if (m_fields->renderer) m_fields->renderer->setVisible(isEnabled);
		if (m_fields->drawNode) m_fields->drawNode->setVisible(isEnabled);
		
		if (showTrailLine && (hitzonesOnDeath || isEnabled)) {
			for (auto& point : playerPath)
				point.lifetime -= dt;

			while (playerPath.size() > 0 && playerPath[0].lifetime <= 0.0f)
				playerPath.erase(playerPath.begin());

			playerPath.push_back({m_player1->getRealPosition(), kPathLifetime});
		}

		if (!isEnabled || !m_fields->renderer)
			return PlayLayer::updateVisibility(dt);

		m_fields->renderer->clear();
		m_fields->drawNode->clear();

		for (int i = 0; i < playerPath.size() - 1; i++) {
			std::array<CCPoint, 2> points = { playerPath[i].point, playerPath[i + 1].point };
			m_fields->drawNode->drawLines(points.data(), 2, 1.0f, {0.0f, 1.0f, 0.0f, 1.0f});
		}

		if (Mod::get()->getSettingValue<bool>("show-player-center-point"))
			m_fields->drawNode->drawCircle(m_player1->getRealPosition(), 2.0f, {1, 0, 0, 1}, 0.0f, {0}, 8);

		std::vector<GameObject*> hazards;
		std::vector<GameObject*> solids;

		for (int i = 0; i < m_activeObjectsCount; i++) {
			GameObject* obj = m_activeObjects[i];

			if (obj == m_anticheatSpike)
				continue;

			switch (obj->m_objectType) {
			case GameObjectType::Hazard:
				hazards.push_back(obj);
				break;
			case GameObjectType::Solid:
			case GameObjectType::Slope:
				solids.push_back(obj);
				break;
			default:
			}
		}

		const CCRect& playerHitbox = m_player1->getObjectRect();
		Box playerBox = Box::fromOBB2D(*m_player1->getOrientedBox());

		std::vector<std::vector<CCPoint>> polygons;

		for (auto obj : solids) {
			CCRect hitbox = obj->getObjectRect();
			
			if (obj->m_objectType == GameObjectType::Slope) {
				auto cornerVec = CCPoint(hitbox.getMaxX(), hitbox.getMinY()) - obj->getRealPosition();
				auto vecX = CCPoint(-hitbox.size.width, 0);
				auto vecY = CCPoint(0, hitbox.size.height);

				auto slopeDir = CCPoint(-1.0f, 1.0f);

				if (obj->slopeFloorTop() == obj->m_slopeUphill) {
					vecX.x = -vecX.x;
					cornerVec.x = -cornerVec.x;
					slopeDir.x = -slopeDir.x;
				}

				if (obj->slopeFloorTop()) {
					vecY.y = -vecY.y;
					cornerVec.y = -cornerVec.y;
					slopeDir.y = -slopeDir.y;
				}

				auto corner = cornerVec + obj->getRealPosition();

				std::vector<CCPoint> points;

				float x = playerHitbox.size.width / 2.0f / hitbox.size.width * hitbox.size.height;

				CCPoint hitboxCorner = corner - slopeDir * playerHitbox.size / 2;
				CCPoint cornerX = corner + vecX + slopeDir * CCPoint(1, -1) * playerHitbox.size / 2;
				CCPoint cornerY = corner + vecY + slopeDir * CCPoint(-1, 1) * playerHitbox.size / 2;

				CCPoint slopeOffset = {0, 0};
				if ((vecX.x < 0) != (vecY.y < 0))
					slopeOffset = -CCPoint(vecY.y, vecX.x).normalize();
				else
					slopeOffset = CCPoint(vecY.y, vecX.x).normalize();

				CCPoint slopeLine0 = corner + vecX + slopeOffset * playerHitbox.size.height / 2;
				CCPoint slopeLine1 = corner + vecY + slopeOffset * playerHitbox.size.height / 2;

				CCPoint slopePointX;
				if (!doIntersect(slopeLine0, slopeLine1, cornerX, cornerX + CCPoint(0, slopeDir.y), slopePointX))
					continue;
				CCPoint slopePointY;
				if (!doIntersect(slopeLine0, slopeLine1, cornerY, cornerY + CCPoint(slopeDir.x, 0), slopePointY))
					continue;

				points.push_back(hitboxCorner);
				points.push_back(cornerX);
				points.push_back(slopePointX);
				points.push_back(slopePointY);
				points.push_back(cornerY);
				
				polygons.push_back(points);
			} else {
				hitbox.origin = hitbox.origin - playerHitbox.size / 2.0f;
				hitbox.size   = hitbox.size + playerHitbox.size;

				if (obj->m_isPassable) {
					if (m_player1->m_isSideways) {
						hitbox.origin += m_player1->m_isUpsideDown ? CCSize(0, 0) : CCSize(hitbox.size.width - 1, 0);
						hitbox.size = CCSize(1, hitbox.size.height);
					} else {
						hitbox.origin += m_player1->m_isUpsideDown ? CCSize(0, 0) : CCSize(0, hitbox.size.height - 1);
						hitbox.size = CCSize(hitbox.size.width, 1);
					}
				}

				polygons.push_back(generateRect(hitbox));
			}
		}

		float fillOpacity = Mod::get()->getSettingValue<float>("fill-opacity");
		float borderOpacity = Mod::get()->getSettingValue<float>("border-opacity");

		ccColor4F solidFillColor    = {kSolidColor,  fillOpacity};
		ccColor4F solidBorderColor  = {kSolidColor,  borderOpacity};
		ccColor4F hazardFillColor   = {kHazardColor, fillOpacity};
		ccColor4F hazardBorderColor = {kHazardColor, borderOpacity};

		for (const auto& polygon : polygons)
			m_fields->renderer->addConvexPolygon(polygon, solidBorderColor);
		for (const auto& polygon : polygons)
			m_fields->renderer->addInsetConvexPolygon(polygon, solidFillColor, kBorderWidth);

		polygons.clear();

		for (auto obj : hazards) {
			if (obj->m_shouldUseOuterOb) {
				Box objBox = Box::fromOBB2D(*obj->getOrientedBox());

				polygons.push_back(calculateMinkowskiDifference(objBox, playerBox));
			} else if (obj->m_objectRadius <= 0.0f) {
				CCRect hitbox = obj->getObjectRect();

				hitbox.origin = hitbox.origin - playerHitbox.size / 2.0f;
				hitbox.size   = hitbox.size + playerHitbox.size;

				polygons.push_back(generateRect(hitbox));
			} else if (m_levelSettings->m_fixRadiusCollision) {
				polygons.push_back(generateCircle(obj->getRealPosition(), obj->m_objectRadius + playerHitbox.size.width / 2));
			} else {
				float plrW2 = playerHitbox.size.width / 2;
				float plrH2 = playerHitbox.size.height / 2;

				polygons.push_back(generateCircle(obj->getRealPosition() + CCPoint(-plrW2, -plrH2), obj->m_objectRadius));
				polygons.push_back(generateCircle(obj->getRealPosition() + CCPoint(-plrW2,  plrH2), obj->m_objectRadius));
				polygons.push_back(generateCircle(obj->getRealPosition() + CCPoint( plrW2, -plrH2), obj->m_objectRadius));
				polygons.push_back(generateCircle(obj->getRealPosition() + CCPoint( plrW2,  plrH2), obj->m_objectRadius));
				polygons.push_back(generateRect(CCRect(obj->getRealPosition() - playerHitbox.size / 2, playerHitbox.size)));
			}
		}

		for (const auto& polygon : polygons)
			m_fields->renderer->addConvexPolygon(polygon, hazardBorderColor);
		for (const auto& polygon : polygons)
			m_fields->renderer->addInsetConvexPolygon(polygon, hazardFillColor, kBorderWidth);

		PlayLayer::updateVisibility(dt);
	}
};

$execute {
    geode::listenForKeybindSettingPresses("toggle-enabled", [](geode::Keybind const& keybind, bool down, bool repeat, double timestamp) {
        if (down && !repeat) {
			hitzonesEnabled = !hitzonesEnabled;
		
			auto alert = TextAlertPopup::create(
				hitzonesEnabled ? "Hitzones: Enabled" : "Hitzones: Disabled",
				0.7f,
				0.5f,
				255,
				"chatFont.fnt"
			);

			alert->setLabelColor(cocos2d::ccWHITE);
			alert->setAlertPosition({0.5f, 1.0f}, {-20.0f, -20.0f});
			CCDirector::get()->getRunningScene()->addChild(alert, 100);
		}
    });
}