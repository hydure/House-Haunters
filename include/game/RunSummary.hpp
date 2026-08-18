#ifndef RUN_SUMMARY_HPP
#define RUN_SUMMARY_HPP

#include <string>

#include "game/Config.hpp"

class RunSummary
{
public:
    static RunSummary& instance();

    void begin(Config::DIFFICULTY difficulty, int players, int rooms);
    void update(float dt);
    void recordDeath();
    void recordDamage(int damage);
    void finish(bool won, int evidenceFound);

    bool complete() const { return complete_; }
    bool won() const { return won_; }
    float elapsedSeconds() const { return elapsedSeconds_; }
    int deaths() const { return deaths_; }
    int damageDealt() const { return damageDealt_; }
    int evidenceFound() const { return evidenceFound_; }
    int players() const { return players_; }
    int rooms() const { return rooms_; }
    Config::DIFFICULTY difficulty() const { return difficulty_; }

    static std::string formatDuration(float seconds);

private:
    Config::DIFFICULTY difficulty_ = Config::NORMAL;
    int players_ = 1;
    int rooms_ = 0;
    float elapsedSeconds_ = 0.f;
    int deaths_ = 0;
    int damageDealt_ = 0;
    int evidenceFound_ = 0;
    bool won_ = false;
    bool complete_ = false;
};

#endif