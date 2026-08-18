#include "game/RunSummary.hpp"

#include <algorithm>
#include <iomanip>
#include <sstream>

RunSummary& RunSummary::instance()
{
    static RunSummary summary;
    return summary;
}

void RunSummary::begin(Config::DIFFICULTY difficulty, int players, int rooms)
{
    difficulty_ = difficulty;
    players_ = std::max(1, players);
    rooms_ = std::max(0, rooms);
    elapsedSeconds_ = 0.f;
    deaths_ = 0;
    damageDealt_ = 0;
    evidenceFound_ = 0;
    won_ = false;
    complete_ = false;
}

void RunSummary::update(float dt)
{
    if (!complete_ && dt > 0.f) elapsedSeconds_ += dt;
}

void RunSummary::recordDeath()
{
    if (!complete_) ++deaths_;
}

void RunSummary::recordDamage(int damage)
{
    if (!complete_) damageDealt_ += std::max(0, damage);
}

void RunSummary::finish(bool won, int evidenceFound)
{
    if (complete_) return;
    won_ = won;
    evidenceFound_ = std::max(0, evidenceFound);
    complete_ = true;
}

std::string RunSummary::formatDuration(float seconds)
{
    const int total = std::max(0, static_cast<int>(seconds));
    std::ostringstream output;
    output << total / 60 << ':' << std::setw(2) << std::setfill('0') << total % 60;
    return output.str();
}