#include "BenchmarkManager.hpp"

#include <DemonsInBetweenAPI.hpp>
#include <Geode/modify/MenuLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/binding/GJGameLevel.hpp>
#include <Geode/ui/Notification.hpp>
#include <Geode/loader/Event.hpp>

using namespace geode::prelude;

static geode::ListenerHandle g_dibListener;

$on_mod(Loaded) {
    g_dibListener = DemonsInBetweenLoadedEvent().listen([](std::map<int, LadderDemon> const& data) {
        BenchmarkManager::get().setDibData(data);
        log::info("GD Benchmark: received {} DIB/GDDL demons", data.size());
        return ListenerResult::Propagate;
    });
}

class $modify(BenchmarkMenuLayer, MenuLayer) {
    bool init() {
        if (!MenuLayer::init()) return false;

        auto winSize = CCDirector::sharedDirector()->getWinSize();
        auto menu = CCMenu::create();
        menu->setID("gd-benchmark-menu");
        menu->setPosition({winSize.width - 55.f, 35.f});

        auto sprite = CCSprite::createWithSpriteFrameName("GJ_button_06.png");
        sprite->setScale(0.75f);
        auto button = CCMenuItemSpriteExtra::create(sprite, this, menu_selector(BenchmarkMenuLayer::onBenchmark));

        auto text = CCLabelBMFont::create("BENCH", "bigFont.fnt");
        text->setScale(0.40f);
        text->setPosition(button->getContentSize() / 2.f);
        button->addChild(text);

        menu->addChild(button);
        this->addChild(menu, 100);
        return true;
    }

    void update(float dt) {
        MenuLayer::update(dt);
        BenchmarkManager::get().update(dt, CCDirector::sharedDirector()->getRunningScene());
    }

    void onBenchmark(CCObject*) {
        BenchmarkManager::get().showMainPopup();
    }
};

class $modify(BenchmarkPlayLayer, PlayLayer) {
    struct Fields {
        CCLabelBMFont* m_benchmarkHUD = nullptr;
    };

    void onEnterTransitionDidFinish() {
        PlayLayer::onEnterTransitionDidFinish();
        auto& mgr = BenchmarkManager::get();
        if (!m_level || !mgr.isBenchmarkLevel(m_level->m_levelID.value())) return;

        auto size = CCDirector::sharedDirector()->getWinSize();
        m_fields->m_benchmarkHUD = CCLabelBMFont::create("BENCHMARK", "bigFont.fnt");
        m_fields->m_benchmarkHUD->setScale(0.48f);
        m_fields->m_benchmarkHUD->setPosition({size.width / 2.f, size.height - 18.f});
        m_fields->m_benchmarkHUD->setZOrder(9999);
        this->addChild(m_fields->m_benchmarkHUD, 9999);
        mgr.onPlayLayerEntered(this);
    }

    void postUpdate(float dt) {
        PlayLayer::postUpdate(dt);
        auto& mgr = BenchmarkManager::get();
        if (!m_level || !mgr.isBenchmarkLevel(m_level->m_levelID.value())) return;

        if (m_isPracticeMode) {
            mgr.markPracticeUsed();
        } else if (!m_isPaused) {
            mgr.update(dt, CCDirector::sharedDirector()->getRunningScene());
        }

        float remain = mgr.remainingSeconds();
        if (m_fields->m_benchmarkHUD) {
            int sec = static_cast<int>(std::ceil(remain));
            int mm = sec / 60;
            int ss = sec % 60;
            m_fields->m_benchmarkHUD->setString(fmt::format("BENCHMARK  {} / {}   {:02d}:{:02d}",
                mgr.currentIndex() + 1,
                mgr.levelCount(),
                mm, ss).c_str());
            m_fields->m_benchmarkHUD->setScale(0.45f);
        }

        if (remain <= 0.f && mgr.state() == BenchmarkManager::State::Playing) {
            mgr.finishCurrentLevel(this);
        }
    }

    void onExit() {
        auto& mgr = BenchmarkManager::get();
        bool benchmark = m_level && mgr.isBenchmarkLevel(m_level->m_levelID.value());
        PlayLayer::onExit();
        if (benchmark) mgr.onPlayLayerExited(this);
    }
};
