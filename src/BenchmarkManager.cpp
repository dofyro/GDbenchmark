#include "BenchmarkManager.hpp"
#include "BenchmarkUI.hpp"
#include "BenchmarkUI.hpp"

#include <Geode/binding/CCMenuItemSpriteExtra.hpp>
#include <Geode/binding/GameLevelManager.hpp>
#include <Geode/binding/GJGameLevel.hpp>
#include <Geode/binding/PlayLayer.hpp>
#include <Geode/ui/Notification.hpp>
#include <Geode/utils/file.hpp>
#include <cocos2d.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <sstream>

using namespace geode::prelude;

namespace {
    constexpr char const* HISTORY_COUNT = "history.count";

    template <class T>
    T saved(std::string const& key, T fallback) {
        return Mod::get()->getSavedValue<T>(key, fallback);
    }

    template <class T>
    void save(std::string const& key, T const& value) {
        Mod::get()->setSavedValue<T>(key, value);
    }

    std::string levelKey(int index, char const* suffix) {
        return fmt::format("history.{}.{}", index, suffix);
    }
}

BenchmarkManager& BenchmarkManager::get() {
    static BenchmarkManager instance;
    return instance;
}

void BenchmarkManager::setDibData(std::map<int, LadderDemon> const& data) {
    m_dibData = data;
}

bool BenchmarkManager::hasDibData() const {
    return !m_dibData.empty();
}

void BenchmarkManager::setConfig(double minTier, double maxTier, int levelCount, int minutes, bool unseenOnly) {
    if (minTier > maxTier) std::swap(minTier, maxTier);
    m_minTier = std::clamp(minTier, 1.0, 39.99);
    m_maxTier = std::clamp(maxTier, m_minTier, 39.99);
    m_levelCount = std::clamp(levelCount, 4, 6);
    m_minutes = std::clamp(minutes, 1, 30);
    m_unseenOnly = unseenOnly;
}

bool BenchmarkManager::isRecentlyUsed(int id, std::set<int> const& used) const {
    return used.contains(id);
}

bool BenchmarkManager::generate() {
    if (!hasDibData()) {
        showNotification("Demons In Between ещё не загрузил GDDL.");
        return false;
    }

    m_selectedIDs.clear();
    m_selectedTiers.clear();
    m_currentUsed.clear();
    m_runs.clear();
    m_currentIndex = -1;
    m_state = State::Idle;

    std::vector<std::pair<int, double>> candidates;
    candidates.reserve(m_dibData.size());

    for (auto const& [id, demon] : m_dibData) {
        if (demon.tier < m_minTier || demon.tier > m_maxTier) continue;
        if (id <= 0) continue;

        if (m_unseenOnly) {
            GJGameLevel* level = nullptr;
            if (loadCachedLevel(id, level) && level && level->getNormalPercent() > 0) {
                continue;
            }
        }

        candidates.emplace_back(id, demon.tier);
    }

    std::shuffle(candidates.begin(), candidates.end(), m_rng);

    if (static_cast<int>(candidates.size()) < m_levelCount) {
        showNotification(fmt::format(
            "Подходящих уровней меньше, чем нужно: {} из {}.",
            candidates.size(), m_levelCount
        ));
        return false;
    }

    for (int i = 0; i < m_levelCount; ++i) {
        m_selectedIDs.push_back(candidates[i].first);
        m_selectedTiers.push_back(candidates[i].second);
    }

    m_state = State::Generated;
    return true;
}

bool BenchmarkManager::loadCachedLevel(int id, GJGameLevel*& out) {
    out = nullptr;
    auto manager = GameLevelManager::get();
    if (!manager) return false;

    // Current GD 2.2081 bindings expose getSavedLevel(int).
    out = manager->getSavedLevel(id);
    return true;
}

bool BenchmarkManager::prepareCurrentLevel() {
    if (m_currentIndex < 0 || m_currentIndex >= static_cast<int>(m_selectedIDs.size())) return false;

    const int id = m_selectedIDs[m_currentIndex];
    GJGameLevel* level = nullptr;
    loadCachedLevel(id, level);

    if (level) {
        if (m_unseenOnly && level->getNormalPercent() > 0) {
            // A cached result may have appeared between generation and launch.
            showNotification("У выбранного уровня уже есть normal-прогресс — пропускаю его.");
            return false;
        }

        BenchmarkLevelRun run;
        run.id = id;
        run.tier = m_selectedTiers[m_currentIndex];
        run.name = level->m_levelName;
        run.baselinePercent = level->getNormalPercent();
        run.baselineAttempts = level->m_attempts.value();
        run.bestPercent = run.baselinePercent;
        run.attempts = 0;
        m_runs.push_back(run);
        return true;
    }

    GameLevelManager::get()->downloadLevel(id, false, 0);
    return false;
}

void BenchmarkManager::start() {
    if (m_selectedIDs.size() < static_cast<size_t>(m_levelCount)) {
        showNotification("Сначала сгенерируй benchmark.");
        return;
    }

    m_runs.clear();
    m_currentIndex = 0;
    m_activeSeconds = 0.0;
    m_downloadWait = 0.f;
    m_currentUsedPractice = false;
    m_hasCurrentRun = false;
    m_leavingByBenchmark = false;
    m_state = State::Downloading;
}

void BenchmarkManager::launchCurrentLevel(CCScene* scene) {
    if (!scene) return;
    if (m_currentIndex < 0 || m_currentIndex >= static_cast<int>(m_selectedIDs.size())) return;

    int const id = m_selectedIDs[m_currentIndex];
    GJGameLevel* level = nullptr;
    loadCachedLevel(id, level);

    if (!level) return;

    if (m_unseenOnly && level->getNormalPercent() > 0) {
        showNotification("Уровень больше не подходит под unseen-фильтр.");
        ++m_currentIndex;
        if (m_currentIndex >= static_cast<int>(m_selectedIDs.size())) {
            finishBenchmark();
            return;
        }
        m_state = State::Downloading;
        m_downloadWait = 0.f;
        return;
    }

    m_currentRun = {};
    m_currentRun.id = id;
    m_currentRun.tier = m_selectedTiers[m_currentIndex];
    m_currentRun.name = level->m_levelName.c_str();
    m_currentRun.baselinePercent = level->getNormalPercent();
    m_currentRun.baselineAttempts = level->m_attempts.value();
    m_currentRun.bestPercent = m_currentRun.baselinePercent;
    m_currentRun.attempts = 0;
    m_hasCurrentRun = true;

    m_activeSeconds = 0.0;
    m_currentUsedPractice = false;
    m_state = State::Playing;
    m_downloadWait = 0.f;

    // Push instead of replace: when the timer finishes, popping PlayLayer reveals
    // the benchmark popup/scene underneath it.
    auto playScene = PlayLayer::scene(level, false, false);
    CCDirector::sharedDirector()->pushScene(playScene);
}

void BenchmarkManager::update(float dt, CCScene* currentScene) {
    if (m_state == State::Playing) {
        m_activeSeconds += std::max(0.f, dt);
        return;
    }
    if (m_state != State::Downloading) return;
    if (!currentScene) return;

    m_downloadWait += dt;
    if (m_downloadWait > 30.f) {
        showNotification("Уровень не удалось скачать за 30 секунд — пропускаю.");
        ++m_currentIndex;
        if (m_currentIndex >= static_cast<int>(m_selectedIDs.size())) {
            finishBenchmark();
        } else {
            m_downloadWait = 0.f;
        }
        return;
    }

    GJGameLevel* level = nullptr;
    loadCachedLevel(m_selectedIDs[m_currentIndex], level);
    if (!level) return;

    // If it is now cached, launch it immediately.
    launchCurrentLevel(currentScene);
}

void BenchmarkManager::onPlayLayerEntered(PlayLayer* layer) {
    if (!layer || m_state != State::Playing) return;
    if (!layer->m_level) return;
    if (!isBenchmarkLevel(layer->m_level->m_levelID.value())) return;
    refreshCurrentLevelLabel();
}

void BenchmarkManager::onPlayLayerExited(PlayLayer* layer) {
    if (!layer) return;
    if (m_state != State::Playing) return;
    if (m_leavingByBenchmark) return;

    // Manual exit / back button aborts the benchmark instead of silently creating
    // a partial observation.
    abort("Уровень был закрыт до окончания времени — benchmark отменён.");
}

bool BenchmarkManager::isBenchmarkLevel(int id) const {
    return m_state == State::Playing &&
           m_currentIndex >= 0 &&
           m_currentIndex < static_cast<int>(m_selectedIDs.size()) &&
           m_selectedIDs[m_currentIndex] == id;
}

float BenchmarkManager::remainingSeconds() const {
    return std::max(0.f, static_cast<float>(m_minutes * 60.0 - m_activeSeconds));
}

void BenchmarkManager::markPracticeUsed() {
    if (m_state == State::Playing) m_currentUsedPractice = true;
}

void BenchmarkManager::refreshCurrentLevelLabel() {
    // Deliberately kept empty for now; PlayLayer creates/updates its own HUD.
}

void BenchmarkManager::finishCurrentLevel(PlayLayer* layer) {
    if (!layer || m_state != State::Playing || !layer->m_level) return;
    if (!m_hasCurrentRun) return;

    auto run = m_currentRun;
    run.id = layer->m_level->m_levelID.value();
    run.bestPercent = layer->m_level->getNormalPercent();
    run.attempts = std::max(0, layer->m_level->m_attempts.value() - run.baselineAttempts);
    run.usedPractice = m_currentUsedPractice;
    m_runs.push_back(run);
    m_hasCurrentRun = false;

    if (m_currentUsedPractice) {
        showNotification("Practice использовался: результат уровня помечен как невалидный.");
    }

    m_leavingByBenchmark = true;
    m_state = State::Generated;
    CCDirector::sharedDirector()->popScene();
    m_leavingByBenchmark = false;

    ++m_currentIndex;
    if (m_currentIndex >= static_cast<int>(m_selectedIDs.size())) {
        finishBenchmark();
        return;
    }

    m_activeSeconds = 0.0;
    m_currentUsedPractice = false;
    m_downloadWait = 0.f;
    m_state = State::Downloading;
}

void BenchmarkManager::finishBenchmark() {
    BenchmarkResult result;
    result.date = today();
    result.minTier = m_minTier;
    result.maxTier = m_maxTier;
    result.durationMinutes = m_minutes;
    result.levels = m_runs;

    double sum = 0.0;
    int valid = 0;
    for (auto const& run : result.levels) {
        if (run.usedPractice) continue;
        sum += run.bestPercent;
        ++valid;
    }

    if (valid == 0) {
        abort("Все уровни были помечены невалидными из-за practice mode.");
        return;
    }

    result.averagePercent = sum / valid;

    double weightedTier = 0.0;
    int weightedCount = 0;
    for (auto const& run : result.levels) {
        if (run.usedPractice) continue;
        weightedTier += run.tier;
        ++weightedCount;
    }

    // First MVP aggregation: reproduce the HTML model's idea, but apply it per
    // benchmark average while keeping every raw observation stored for later.
    double meanTier = weightedCount ? weightedTier / weightedCount : m_minTier;
    result.rating = estimateRating(meanTier, result.averagePercent);

    m_lastResult = result;
    saveResult(result);
    m_state = State::Finished;
    showNotification(fmt::format(
        "Benchmark готов: {:.2f} rating · {:.1f}% average",
        result.rating, result.averagePercent
    ));
}

void BenchmarkManager::abort(std::string const& reason) {
    m_state = State::Aborted;
    m_selectedIDs.clear();
    m_selectedTiers.clear();
    m_runs.clear();
    m_currentIndex = -1;
    m_activeSeconds = 0.0;
    m_currentUsedPractice = false;
    m_hasCurrentRun = false;
    showNotification(reason);
}

void BenchmarkManager::resetSession() {
    m_state = State::Idle;
    m_selectedIDs.clear();
    m_selectedTiers.clear();
    m_runs.clear();
    m_currentIndex = -1;
}

double BenchmarkManager::estimateRating(double tier, double percent) {
    double p = std::clamp(percent, 1.0, 99.0) / 100.0;
    double logit = std::log(p / (1.0 - p));
    return std::round((tier + logit * 0.25) * 100.0) / 100.0;
}

std::string BenchmarkManager::today() {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#if defined(_WIN32)
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    std::ostringstream out;
    out << std::put_time(&tm, "%Y-%m-%d");
    return out.str();
}

std::string BenchmarkManager::serializeLevels(BenchmarkResult const& result) {
    std::ostringstream out;
    bool first = true;
    for (auto const& run : result.levels) {
        if (!first) out << ';';
        first = false;
        out << run.id << ',' << std::fixed << std::setprecision(2) << run.tier << ','
            << run.bestPercent << ',' << run.attempts << ',' << (run.usedPractice ? 1 : 0);
    }
    return out.str();
}

std::vector<BenchmarkLevelRun> BenchmarkManager::parseLevels(std::string const& value) {
    std::vector<BenchmarkLevelRun> result;
    std::stringstream ss(value);
    std::string item;
    while (std::getline(ss, item, ';')) {
        if (item.empty()) continue;
        std::stringstream row(item);
        std::string part;
        std::vector<std::string> parts;
        while (std::getline(row, part, ',')) parts.push_back(part);
        if (parts.size() < 5) continue;
        try {
            BenchmarkLevelRun run;
            run.id = std::stoi(parts[0]);
            run.tier = std::stod(parts[1]);
            run.bestPercent = std::stoi(parts[2]);
            run.attempts = std::stoi(parts[3]);
            run.usedPractice = std::stoi(parts[4]) != 0;
            result.push_back(run);
        } catch (...) {}
    }
    return result;
}

void BenchmarkManager::saveResult(BenchmarkResult const& result) {
    int count = saved<int>(HISTORY_COUNT, 0);
    int index = std::min(count, 99);

    if (count >= 100) {
        for (int i = 1; i < 100; ++i) {
            for (auto const* suffix : {"date", "rating", "avg", "min", "max", "minutes", "levels"}) {
                auto old = levelKey(i, suffix);
                auto neu = levelKey(i - 1, suffix);
                if (std::string(suffix) == "rating" || std::string(suffix) == "avg" ||
                    std::string(suffix) == "min" || std::string(suffix) == "max") {
                    save<double>(neu, saved<double>(old, 0.0));
                } else if (std::string(suffix) == "minutes") {
                    save<int>(neu, saved<int>(old, 10));
                } else if (std::string(suffix) == "date" || std::string(suffix) == "levels") {
                    save<std::string>(neu, saved<std::string>(old, ""));
                }
            }
        }
        index = 99;
    } else {
        ++count;
        save<int>(HISTORY_COUNT, count);
    }

    save<std::string>(levelKey(index, "date"), result.date);
    save<double>(levelKey(index, "rating"), result.rating);
    save<double>(levelKey(index, "avg"), result.averagePercent);
    save<double>(levelKey(index, "min"), result.minTier);
    save<double>(levelKey(index, "max"), result.maxTier);
    save<int>(levelKey(index, "minutes"), result.durationMinutes);
    save<std::string>(levelKey(index, "levels"), serializeLevels(result));
}

std::vector<BenchmarkResult> BenchmarkManager::loadHistory() const {
    int count = saved<int>(HISTORY_COUNT, 0);
    std::vector<BenchmarkResult> result;
    result.reserve(count);

    for (int i = 0; i < count; ++i) {
        BenchmarkResult entry;
        entry.date = saved<std::string>(levelKey(i, "date"), "");
        entry.rating = saved<double>(levelKey(i, "rating"), 0.0);
        entry.averagePercent = saved<double>(levelKey(i, "avg"), 0.0);
        entry.minTier = saved<double>(levelKey(i, "min"), 0.0);
        entry.maxTier = saved<double>(levelKey(i, "max"), 0.0);
        entry.durationMinutes = saved<int>(levelKey(i, "minutes"), 10);
        entry.levels = parseLevels(saved<std::string>(levelKey(i, "levels"), ""));
        result.push_back(entry);
    }

    return result;
}

BenchmarkResult const* BenchmarkManager::lastResult() const {
    if (!m_lastResult.date.empty()) return &m_lastResult;
    return nullptr;
}

void BenchmarkManager::showMainPopup() {
    showBenchmarkPopup();
}

void BenchmarkManager::showHistoryPopup() {
    showBenchmarkHistoryPopup();
}

void BenchmarkManager::showNotification(std::string const& text) {
    Notification::create(text, NotificationIcon::Info)->show();
}
