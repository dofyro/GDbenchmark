#include "BenchmarkUI.hpp"
#include "BenchmarkManager.hpp"

#include <Geode/ui/Popup.hpp>
#include <Geode/ui/Notification.hpp>
#include <Geode/binding/CCMenuItemSpriteExtra.hpp>
#include <Geode/binding/GJGameLevel.hpp>
#include <Geode/binding/GameLevelManager.hpp>

using namespace geode::prelude;

namespace {
    CCMenuItemLabel* textButton(char const* text, cocos2d::CCObject* target, cocos2d::SEL_MenuHandler selector) {
        auto label = CCLabelBMFont::create(text, "bigFont.fnt");
        label->setScale(0.55f);
        return CCMenuItemLabel::create(label, target, selector);
    }

    CCLabelBMFont* label(char const* text, float scale = 0.55f) {
        auto result = CCLabelBMFont::create(text, "bigFont.fnt");
        result->setScale(scale);
        return result;
    }

    std::string prettyTier(double tier) {
        return fmt::format("{:.2f}", tier);
    }
}

class BenchmarkPopup : public geode::Popup {
private:
    CCLabelBMFont* m_minLabel = nullptr;
    CCLabelBMFont* m_maxLabel = nullptr;
    CCLabelBMFont* m_countLabel = nullptr;
    CCLabelBMFont* m_minutesLabel = nullptr;
    CCLabelBMFont* m_unseenLabel = nullptr;
    CCLabelBMFont* m_status = nullptr;
    CCLabelBMFont* m_levels = nullptr;

    void refresh() {
        auto& mgr = BenchmarkManager::get();
        m_minLabel->setString(fmt::format("{:.2f}", mgr.minTier()).c_str());
        m_maxLabel->setString(fmt::format("{:.2f}", mgr.maxTier()).c_str());
        m_countLabel->setString(fmt::format("{}", mgr.levelCount()).c_str());
        m_minutesLabel->setString(fmt::format("{} min", mgr.minutes()).c_str());
        m_unseenLabel->setString(mgr.unseenOnly() ? "UNSEEN: ON" : "UNSEEN: OFF");

        std::string status;
        if (!mgr.hasDibData()) {
            status = "Waiting for Demons In Between / GDDL…";
        } else if (mgr.state() == BenchmarkManager::State::Finished && mgr.lastResult()) {
            auto r = *mgr.lastResult();
            status = fmt::format("Last: {:.2f} rating · {:.1f}%", r.rating, r.averagePercent);
        } else {
            status = "Ready to generate a test.";
        }
        m_status->setString(status.c_str());

        std::string levels;
        auto const& ids = mgr.selectedIDs();
        auto tiers = mgr.selectedTiers();
        if (ids.empty()) {
            levels = "No benchmark generated yet.";
        } else {
            for (size_t i = 0; i < ids.size(); ++i) {
                GJGameLevel* level = GameLevelManager::get()->getSavedLevel(ids[i]);
                std::string name = level ? std::string(level->m_levelName) : fmt::format("ID {}", ids[i]);
                levels += fmt::format("{}  ·  {}  {}\n", i + 1, prettyTier(tiers[i]), name);
            }
        }
        m_levels->setString(levels.c_str());
    }

    void adjustMin(int dir) {
        auto& mgr = BenchmarkManager::get();
        mgr.setConfig(mgr.minTier() + dir * 0.05, mgr.maxTier(), mgr.levelCount(), mgr.minutes(), mgr.unseenOnly());
        refresh();
    }

    void adjustMax(int dir) {
        auto& mgr = BenchmarkManager::get();
        mgr.setConfig(mgr.minTier(), mgr.maxTier() + dir * 0.05, mgr.levelCount(), mgr.minutes(), mgr.unseenOnly());
        refresh();
    }

    void adjustCount(int dir) {
        auto& mgr = BenchmarkManager::get();
        mgr.setConfig(mgr.minTier(), mgr.maxTier(), mgr.levelCount() + dir, mgr.minutes(), mgr.unseenOnly());
        refresh();
    }

    void adjustMinutes(int dir) {
        auto& mgr = BenchmarkManager::get();
        mgr.setConfig(mgr.minTier(), mgr.maxTier(), mgr.levelCount(), mgr.minutes() + dir, mgr.unseenOnly());
        refresh();
    }

    void toggleUnseen(CCObject*) {
        auto& mgr = BenchmarkManager::get();
        mgr.setConfig(mgr.minTier(), mgr.maxTier(), mgr.levelCount(), mgr.minutes(), !mgr.unseenOnly());
        refresh();
    }

    void generate(CCObject*) {
        if (BenchmarkManager::get().generate()) refresh();
    }

    void start(CCObject*) {
        auto& mgr = BenchmarkManager::get();
        mgr.start();
        if (mgr.state() == BenchmarkManager::State::Downloading) {
            this->setVisible(false);
        }
    }

    void history(CCObject*) {
        showBenchmarkHistoryPopup();
    }

protected:
    bool init() {
        if (!Popup::init(420.f, 360.f)) return false;
        this->setTitle("GD Benchmark");
        auto win = this->getContentSize();

        auto title = label("Skill benchmark · DIB/GDDL", 0.42f);
        title->setPosition({win.width / 2.f, win.height - 36.f});
        this->addChild(title);

        auto addControlRow = [&](float y, char const* caption, CCLabelBMFont*& value, SEL_MenuHandler minus, SEL_MenuHandler plus) {
            auto cap = label(caption, 0.48f);
            cap->setAnchorPoint({0.f, 0.5f});
            cap->setPosition({35.f, y});
            this->addChild(cap);

            value = label("?", 0.52f);
            value->setPosition({win.width / 2.f, y});
            this->addChild(value);

            auto menu = CCMenu::create();
            menu->setPosition({0.f, 0.f});
            menu->addChild(textButton("−", this, minus));
            menu->addChild(textButton("+", this, plus));
            auto items = menu->getChildren();
            static_cast<CCMenuItem*>(items->objectAtIndex(0))->setPosition({win.width / 2.f - 65.f, y});
            static_cast<CCMenuItem*>(items->objectAtIndex(1))->setPosition({win.width / 2.f + 65.f, y});
            this->addChild(menu);
        };

        addControlRow(win.height - 72.f, "MIN TIER", m_minLabel,
            menu_selector(BenchmarkPopup::minusMin), menu_selector(BenchmarkPopup::plusMin));
        addControlRow(win.height - 104.f, "MAX TIER", m_maxLabel,
            menu_selector(BenchmarkPopup::minusMax), menu_selector(BenchmarkPopup::plusMax));
        addControlRow(win.height - 136.f, "LEVELS", m_countLabel,
            menu_selector(BenchmarkPopup::minusCount), menu_selector(BenchmarkPopup::plusCount));
        addControlRow(win.height - 168.f, "TIME", m_minutesLabel,
            menu_selector(BenchmarkPopup::minusMinutes), menu_selector(BenchmarkPopup::plusMinutes));

        m_unseenLabel = label("UNSEEN: ON", 0.46f);
        m_unseenLabel->setPosition({win.width / 2.f, win.height - 196.f});
        this->addChild(m_unseenLabel);
        auto unseenMenu = CCMenu::create();
        unseenMenu->setPosition({0.f, 0.f});
        auto unseenBtn = textButton("toggle", this, menu_selector(BenchmarkPopup::onToggleUnseen));
        unseenBtn->setPosition({win.width / 2.f + 85.f, win.height - 196.f});
        unseenMenu->addChild(unseenBtn);
        this->addChild(unseenMenu);

        m_status = label("", 0.42f);
        m_status->setColor({180, 180, 190});
        m_status->setPosition({win.width / 2.f, win.height - 220.f});
        this->addChild(m_status);

        m_levels = label("", 0.36f);
        m_levels->setAnchorPoint({0.f, 1.f});
        m_levels->setPosition({32.f, win.height - 238.f});
        this->addChild(m_levels);

        auto bottom = CCMenu::create();
        bottom->setPosition({win.width / 2.f, 28.f});
        auto gen = textButton("GENERATE", this, menu_selector(BenchmarkPopup::onGenerate));
        auto start = textButton("START", this, menu_selector(BenchmarkPopup::onStart));
        auto hist = textButton("HISTORY", this, menu_selector(BenchmarkPopup::onHistory));
        gen->setPosition({-100.f, 0.f});
        start->setPosition({0.f, 0.f});
        hist->setPosition({100.f, 0.f});
        bottom->addChild(gen);
        bottom->addChild(start);
        bottom->addChild(hist);
        this->addChild(bottom);

        refresh();
        return true;
    }

    // Selector callbacks used by the popup buttons.
    void minusMin(CCObject*) { adjustMin(-1); }
    void plusMin(CCObject*) { adjustMin(+1); }
    void minusMax(CCObject*) { adjustMax(-1); }
    void plusMax(CCObject*) { adjustMax(+1); }
    void minusCount(CCObject*) { adjustCount(-1); }
    void plusCount(CCObject*) { adjustCount(+1); }
    void minusMinutes(CCObject*) { adjustMinutes(-1); }
    void plusMinutes(CCObject*) { adjustMinutes(+1); }
    void onToggleUnseen(CCObject* sender) { toggleUnseen(sender); }
    void onGenerate(CCObject* sender) { generate(sender); }
    void onStart(CCObject* sender) { start(sender); }
    void onHistory(CCObject* sender) { history(sender); }
};

class BenchmarkHistoryPopup : public geode::Popup {
protected:
    bool init() {
        if (!Popup::init(420.f, 300.f)) return false;
        this->setTitle("Benchmark History");
        auto win = this->getContentSize();
        auto history = BenchmarkManager::get().loadHistory();

        if (history.empty()) {
            auto empty = label("No benchmark results yet.", 0.55f);
            empty->setPosition({win.width / 2.f, win.height / 2.f});
            this->addChild(empty);
            return true;
        }

        float y = win.height - 44.f;
        for (auto it = history.rbegin(); it != history.rend() && y > 30.f; ++it, y -= 30.f) {
            auto const& r = *it;
            auto row = label(fmt::format("{}   {:.2f} rating   {:.1f}%   [{:.2f}–{:.2f}]",
                r.date, r.rating, r.averagePercent, r.minTier, r.maxTier).c_str(), 0.39f);
            row->setAnchorPoint({0.f, 0.5f});
            row->setPosition({24.f, y});
            this->addChild(row);
        }

        auto note = label("Raw per-level observations are retained in saved data for later graph/stat work.", 0.35f);
        note->setPosition({win.width / 2.f, 18.f});
        this->addChild(note);
        return true;
    }
};

void showBenchmarkPopup() {
    auto ret = new BenchmarkPopup();
    if (ret && ret->init()) {
        ret->autorelease();
        ret->show();
        return;
    }
    CC_SAFE_DELETE(ret);
}

void showBenchmarkHistoryPopup() {
    auto ret = new BenchmarkHistoryPopup();
    if (ret && ret->init()) {
        ret->autorelease();
        ret->show();
        return;
    }
    CC_SAFE_DELETE(ret);
}
