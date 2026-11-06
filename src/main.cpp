#include <Geode/Geode.hpp>
#include <Geode/modify/EditorUI.hpp>
#include <random>
#include <sstream>
#include <vector>
using namespace geode::prelude;

static std::vector<int> parseIds(std::string const& s) {
    std::vector<int> out;
    std::stringstream ss(s);
    std::string t;
    while (std::getline(ss, t, ',')) {
        try { int v = std::stoi(t); if (v > 0) out.push_back(v); } catch (...) {}
    }
    if (out.empty()) out.push_back(1);
    return out;
}

static ZLayer parseLayer(std::string const& s) {
    if (s == "B4") return ZLayer::B4;
    if (s == "B3") return ZLayer::B3;
    if (s == "B1") return ZLayer::B1;
    if (s == "T1") return ZLayer::T1;
    if (s == "T2") return ZLayer::T2;
    if (s == "T3") return ZLayer::T3;
    return ZLayer::B2;
}

class $modify(AutoDecoUI, EditorUI) {
    bool init(LevelEditorLayer* lel) {
        if (!EditorUI::init(lel)) return false;
        auto spr = ButtonSprite::create("Auto Deco");
        spr->setScale(0.6f);
        auto btn = CCMenuItemSpriteExtra::create(spr, this, menu_selector(AutoDecoUI::onAutoDeco));
        auto menu = CCMenu::create();
        menu->addChild(btn);
        auto win = CCDirector::get()->getWinSize();
        menu->setPosition({win.width - 55.f, win.height - 75.f});
        menu->setID("auto-deco-menu"_spr);
        this->addChild(menu, 100);
        return true;
    }

    void onAutoDeco(CCObject*) {
        auto sel = this->getSelectedObjects();
        if (!sel || sel->count() == 0) {
            FLAlertLayer::create("Auto Deco", "Wähle zuerst Objekte aus.", "OK")->show();
            return;
        }
        auto mod = Mod::get();
        bool edge = mod->getSettingValue<bool>("edge-glow");
        bool edgeB = mod->getSettingValue<bool>("edge-bottom");
        bool pillars = mod->getSettingValue<bool>("pillars");
        bool corners = mod->getSettingValue<bool>("corners");
        bool scatter = mod->getSettingValue<bool>("scatter");
        bool hazards = mod->getSettingValue<bool>("hazards");
        int density = (int)mod->getSettingValue<int64_t>("density");
        float size = (float)mod->getSettingValue<double>("size");
        float off = (float)mod->getSettingValue<double>("offset");
        float radius = (float)mod->getSettingValue<double>("scatter-radius");
        auto ids = parseIds(mod->getSettingValue<std::string>("deco-ids"));
        int colorId = (int)mod->getSettingValue<int64_t>("color-id");
        auto layer = parseLayer(mod->getSettingValue<std::string>("z-layer"));
        bool noTouch = mod->getSettingValue<bool>("no-touch");
        int seed = (int)mod->getSettingValue<int64_t>("seed");
        int maxObj = (int)mod->getSettingValue<int64_t>("max-objects");

        std::mt19937 rng(seed ? (unsigned)seed : std::random_device{}());
        auto rnd = [&](float a, float b) { return std::uniform_real_distribution<float>(a, b)(rng); };
        auto pickId = [&]() { return ids[rng() % ids.size()]; };

        struct Src { CCRect r; GameObjectType t; };
        std::vector<Src> srcs;
        for (auto o : CCArrayExt<GameObject*>(sel)) {
            srcs.push_back({o->getObjectRect(), o->m_objectType});
        }

        auto created = CCArray::create();
        auto place = [&](int id, CCPoint p, float scale) {
            if ((int)created->count() >= maxObj) return;
            auto o = m_editorLayer->createObject(id, p, true);
            if (!o) return;
            o->updateCustomScaleX(scale);
            o->updateCustomScaleY(scale);
            o->m_zLayer = layer;
            if (noTouch) o->m_isNoTouch = true;
            if (colorId > 0 && o->m_baseColor) o->m_baseColor->m_colorID = colorId;
            created->addObject(o);
        };

        for (auto& s : srcs) {
            float l = s.r.getMinX(), r = s.r.getMaxX();
            float b = s.r.getMinY(), t = s.r.getMaxY();
            float w = r - l;
            if (s.t == GameObjectType::Solid) {
                if (edge || edgeB) {
                    int n = std::max(1, (int)std::round(density * w / 30.f));
                    for (int i = 0; i < n; i++) {
                        float x = l + (i + 0.5f) * w / n;
                        if (edge) place(pickId(), {x, t + off}, size);
                        if (edgeB) place(pickId(), {x, b - off}, size);
                    }
                }
                if (corners) {
                    float in = 3.f;
                    place(pickId(), {l + in, t - in}, size);
                    place(pickId(), {r - in, t - in}, size);
                    place(pickId(), {l + in, b + in}, size);
                    place(pickId(), {r - in, b + in}, size);
                }
                if (pillars && b > 1.f) {
                    float stride = std::max(6.f, 30.f * size);
                    int cols = std::clamp(density / 4 + 1, 1, 3);
                    for (int c = 0; c < cols; c++) {
                        float x = l + (c + 0.5f) * w / cols;
                        for (float y = b - stride / 2 - off; y > 0.f; y -= stride)
                            place(ids[0], {x, y}, size);
                    }
                }
                if (scatter) {
                    int n = density / 2 + 1;
                    for (int i = 0; i < n; i++)
                        place(pickId(), {rnd(l - radius, r + radius), rnd(b - radius, t + radius)}, size * rnd(0.5f, 1.5f));
                }
            } else if (hazards && s.t == GameObjectType::Hazard) {
                for (int i = 0; i < 3; i++)
                    place(pickId(), {rnd(l - radius, r + radius), rnd(b - radius, t + radius)}, size * rnd(0.5f, 1.2f));
            }
        }

        this->deselectAll();
        if (created->count()) this->selectObjects(created, true);
        Notification::create(fmt::format("Auto Deco: {} Objekte erstellt", created->count()),
                             NotificationIcon::Success)->show();
    }
};
