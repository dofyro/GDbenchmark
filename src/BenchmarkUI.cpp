#include "BenchmarkUI.hpp"
#include "BenchmarkManager.hpp"

#include <Geode/ui/Popup.hpp>
#include <Geode/ui/Notification.hpp>
#include <Geode/binding/CCMenuItemSpriteExtra.hpp>
#include <Geode/binding/CCTextInputNode.hpp>
#include <Geode/binding/GJGameLevel.hpp>
#include <Geode/binding/GameLevelManager.hpp>

using namespace geode::prelude;

namespace {
    CCMenuItemLabel* textButton(char const* text, cocos2d::CCObject* target, cocos2d::SEL_MenuHandler selector) {
        auto label = CCLabelBMFont::create(text, "bigFont.fnt");
        label->setScale(0.48f);
        return CCMenuItemLabel::create(label, target, selector);
    }

    CCLabelBMFont* label(char const* text, float scale = 0.55f) {
        auto result = CCLabelBMFont::create(text, "bigFont.fnt");
        result->setScale(scale);
        return result;
    }
}

class BenchmarkPopup : public geode::Popup, public TextInputDelegate {
private:
    CCTextInputNode* m_minInput = nullptr;
    CCTextInputNode* m_maxInput = nullptr;
    CCLabelBMFont* m_countLabel = nullptr;
    CCLabelBMFont* m_minutesLabel = nullptr;
    CCLabelBMFont* m_unseenLabel = nullptr;
    CCLabelBMFont* m_status = nullptr;
    bool m_syncingInputs = false;

    bool parseTier(CCTextInputNode* input, double& out) const {
        if (!input) return false;
        auto value = std::string(input->getString());
        if (value.empty() || value == ".") return false;
        try {
            size_t consumed = 0;
            out = std::stod(value, &consumed);
            return consumed == value.size();
        } catch (...) {
            return false;
        }
    }

    void syncConfigFromInputs(CCTextInputNode* changed) {
        if (m_syncingInputs) return;

        double minTier = 0.0;
        double maxTier = 0.0;
        if (!parseTier(m_minInput, minTier) || !parseTier(m_maxInput, maxTier)) return;

        minTier = std::clamp(minTier, 1.0, 39.99);
        maxTier = std::clamp(maxTier, 1.0, 39.99);

        if (changed == m_minInput && minTier > maxTier) {
            maxTier = minTier;
            m_syncingInputs = true;
            m_maxInput->setString(fmt::format("{:.2f}", maxTier));
            m_syncingInputs = false;
        } else if (changed == m_maxInput && maxTier < minTier) {
            minTier = maxTier;
            m_syncingInputs = true;
            m_minInput->setString(fmt::format("{:.2f}", minTier));
            m_syncingInputs = false;
        }

        auto& mgr = BenchmarkManager::get();
        mgr.setConfig(minTier, maxTier, mgr.levelCount(), mgr.minutes(), mgr.unseenOnly());
        refresh();
    }

    void refresh() {
        auto& mgr = BenchmarkManager::get();

        m_syncingInputs = true;
        m_minInput->setString(fmt::format("{:.2f}", mgr.minTier()));
        m_maxInput->setString(fmt::format("{:.2f}", mgr.maxTier()));
        m_syncingInputs = false;

        m_countLabel->setString(fmt::format("{}", mgr.levelCount()).c_str());
        m_minutesLabel->setString(fmt::format("{} min", mgr.minutes()).c_str());
        m_unseenLabel->setString(mgr.unseenOnly() ? "UNSEEN: ON" : "UNSEEN: OFF");

        std::string status;
        if (!mgr.hasDibData()) {
            status = "Waiting for Demons In Between / GDDL…";
        } else if (mgr.state() == BenchmarkManager::State::Finished && mgr.lastResult()) {
            auto r = *mgr.lastResult();
            status = fmt::format("Last: {:.2f} rating · {:.1f}%", r.rating, r.averagePercent);
        } else if (mgr.state() == BenchmarkManager::State::Generated) {
            status = fmt::format("{} levels ready. START opens the first one.", mgr.levelCount());
        } else {
            status = "Set the range, then generate a benchmark.";
        }
        m_status->setString(status.c_str());
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
        if (mgr.state() != BenchmarkManager::State::Downloading) return;

        // Queue every selected level now. The manager will open the first level as
        // soon as GD has cached it, then continue with the remaining levels.
        auto const& ids = mgr.selectedIDs();
        for (auto const id : ids) {
            if (auto manager = GameLevelManager::get()) {
                manager->downloadLevel(id, false, 0);
            }
        }

        this->setVisible(false);
    }

    void history(CCObject*) {
        showBenchmarkHistoryPopup();
    }

protected:
    bool init() {
        if (!Popup::init(420.f, 320.f)) return false;
        this->setTitle("GD Benchmark");
        auto win = this->getContentSize();

        auto title = label("Skill benchmark · DIB/GDDL", 0.42f);
        title->setPosition({win.width / 2.f, win.height - 38.f});
        this->addChild(title);

        auto addInputRow = [&](float y, char const* caption, CCTextInputNode*& input, char const* initial) {
            auto cap = label(caption, 0.46f);
            cap->setAnchorPoint({0.f, 0.5f});
            cap->setPosition({28.f, y});
            this->addChild(cap);

            input = CCTextInputNode::create(92.f, 26.f, "", "bigFont.fnt");
            input->setAllowedChars("0123456789.");
            input->setMaxLabelLength(6);
            input->setString(initial);
            input->setDelegate(this);
            input->setPosition({win.width / 2.f, y});
            this->addChild(input);
        };

        addInputRow(win.height - 76.f, "MIN TIER", m_minInput, "14.00");
        addInputRow(win.height - 110.f, "MAX TIER", m_maxInput, "14.50");

        auto addCounterRow = [&](float y, char const* caption, CCLabelBMFont*& value, SEL_MenuHandler minus, SEL_MenuHandler plus) {
            auto cap = label(caption, 0.46f);
            cap->setAnchorPoint({0.f, 0.5f});
            cap->setPosition({28.f, y});
            this->addChild(cap);

            value = label("?", 0.50f);
            value->setPosition({win.width / 2.f, y});
            this->addChild(value);

            auto menu = CCMenu::create();
            menu->setPosition({0.f, 0.f});
            auto minusBtn = textButton("−", this, minus);
            auto plusBtn = textButton("+", this, plus);
            minusBtn->setPosition({win.width / 2.f - 72.f, y});
            plusBtn->setPosition({win.width / 2.f + 72.f, y});
            menu->addChild(minusBtn);
            menu->addChild(plusBtn);
            this->addChild(menu);
        };

        addCounterRow(win.height - 144.f, "LEVELS", m_countLabel,
            menu_selector(BenchmarkPopup::minusCount), menu_selector(BenchmarkPopup::plusCount));
        addCounterRow(win.height - 178.f, "TIME", m_minutesLabel,
            menu_selector(BenchmarkPopup::minusMinutes), menu_selector(BenchmarkPopup::plusMinutes));

        m_unseenLabel = label("UNSEEN: ON", 0.43f);
        m_unseenLabel->setPosition({win.width / 2.f - 25.f, win.height - 210.f});
        this->addChild(m_unseenLabel);

        auto unseenMenu = CCMenu::create();
        unseenMenu->setPosition({0.f, 0.f});
        auto unseenBtn = textButton("toggle", this, menu_selector(BenchmarkPopup::onToggleUnseen));
        unseenBtn->setPosition({win.width / 2.f + 110.f, win.height - 210.f});
        unseenMenu->addChild(unseenBtn);
        this->addChild(unseenMenu);

        m_status = label("", 0.38f);
        m_status->setPosition({win.width / 2.f, win.height - 242.f});
        this->addChild(m_status);

        auto note = label("START loads the selected benchmark one level at a time.\n"
                          "After the timer ends, the next selected level opens automatically.", 0.33f);
        note->setAlignment(kCCTextAlignmentCenter);
        note->setDimensions({win.width - 56.f, 42.f});
        note->setPosition({win.width / 2.f, 54.f});
        this->addChild(note);

        auto bottom = CCMenu::create();
        bottom->setPosition({win.width / 2.f, 22.f});
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

    void textChanged(CCTextInputNode* node) override {
        if (m_syncingInputs) return;
        if (node == m_minInput || node == m_maxInput) {
            syncConfigFromInputs(node);
        }
    }

    void textInputOpened(CCTextInputNode*) override {}
    void textInputClosed(CCTextInputNode*) override {}

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
