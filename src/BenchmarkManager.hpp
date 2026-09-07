#pragma once

#include <Geode/Geode.hpp>
#include <DemonsInBetweenAPI.hpp>
#include <map>
#include <random>
#include <set>
#include <string>
#include <vector>

struct BenchmarkLevelRun {
    int id = 0;
    double tier = 0.0;
    std::string name;
    int baselinePercent = 0;
    int baselineAttempts = 0;
    int bestPercent = 0;
    int attempts = 0;
    bool usedPractice = false;
};

struct BenchmarkResult {
    std::string date;
    double minTier = 0.0;
    double maxTier = 0.0;
    int durationMinutes = 10;
    double averagePercent = 0.0;
    double rating = 0.0;
    std::vector<BenchmarkLevelRun> levels;
};

class BenchmarkManager {
public:
    enum class State {
        Idle,
        Generated,
        Downloading,
        Playing,
        Finished,
        Aborted,
    };

    static BenchmarkManager& get();

    void setDibData(std::map<int, LadderDemon> const& data);
    bool hasDibData() const;

    void setConfig(double minTier, double maxTier, int levelCount, int minutes, bool unseenOnly);
    double minTier() const { return m_minTier; }
    double maxTier() const { return m_maxTier; }
    int levelCount() const { return m_levelCount; }
    int minutes() const { return m_minutes; }
    bool unseenOnly() const { return m_unseenOnly; }

    bool generate();
    std::vector<int> const& selectedIDs() const { return m_selectedIDs; }
    std::vector<double> selectedTiers() const { return m_selectedTiers; }

    void start();
    void update(float dt, cocos2d::CCScene* currentScene);
    void onPlayLayerEntered(class PlayLayer* layer);
    void onPlayLayerExited(class PlayLayer* layer);
    void abort(std::string const& reason);

    bool isBenchmarkLevel(int id) const;
    bool active() const { return m_state == State::Playing || m_state == State::Downloading; }
    State state() const { return m_state; }
    int currentIndex() const { return m_currentIndex; }
    float remainingSeconds() const;
    bool currentUsedPractice() const { return m_currentUsedPractice; }

    void markPracticeUsed();
    void refreshCurrentLevelLabel();

    BenchmarkResult const* lastResult() const;
    std::vector<BenchmarkResult> loadHistory() const;
    void saveResult(BenchmarkResult const& result);

    static double estimateRating(double tier, double percent);
    static std::string today();
    static std::string serializeLevels(BenchmarkResult const& result);
    static std::vector<BenchmarkLevelRun> parseLevels(std::string const& value);

    void showMainPopup();
    void showHistoryPopup();
    void showNotification(std::string const& text);

    // Called from the PlayLayer hook after the test time expires.
    void finishCurrentLevel(class PlayLayer* layer);

private:
    BenchmarkManager() = default;

    bool prepareCurrentLevel();
    void launchCurrentLevel(cocos2d::CCScene* scene);
    bool loadCachedLevel(int id, class GJGameLevel*& out);
    bool isRecentlyUsed(int id, std::set<int> const& used) const;
    void finishBenchmark();
    void resetSession();

    std::map<int, LadderDemon> m_dibData;
    std::vector<int> m_selectedIDs;
    std::vector<double> m_selectedTiers;
    std::vector<BenchmarkLevelRun> m_runs;
    std::set<int> m_currentUsed;

    std::mt19937 m_rng{std::random_device{}()};

    double m_minTier = 14.0;
    double m_maxTier = 14.5;
    int m_levelCount = 6;
    int m_minutes = 10;
    bool m_unseenOnly = true;

    State m_state = State::Idle;
    int m_currentIndex = -1;
    double m_activeSeconds = 0.0;
    BenchmarkLevelRun m_currentRun;
    bool m_hasCurrentRun = false;
    bool m_currentUsedPractice = false;
    bool m_leavingByBenchmark = false;
    float m_downloadWait = 0.f;
    BenchmarkResult m_lastResult;
};
