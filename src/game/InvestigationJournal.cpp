#include "game/InvestigationJournal.hpp"

#include <algorithm>

InvestigationJournal& InvestigationJournal::instance()
{
    static InvestigationJournal journal;
    return journal;
}

void InvestigationJournal::reset()
{
    entries_.clear();
}

bool InvestigationJournal::discover(const std::string& text,
                                    Tier tier,
                                    int discoveredBy)
{
    const auto duplicate = std::find_if(entries_.begin(), entries_.end(),
        [&](const Entry& entry) {
            return entry.text == text && entry.tier == tier;
        });
    if (duplicate != entries_.end()) {
        return false;
    }

    entries_.push_back({text, tier, discoveredBy});
    return true;
}

const char* InvestigationJournal::tierName(Tier tier)
{
    switch (tier) {
        case Tier::WORTHLESS: return "WORTHLESS";
        case Tier::VAGUE:    return "VAGUE";
        case Tier::SPECIFIC: return "SPECIFIC";
        case Tier::JACKPOT:  return "ITEM";
    }
    return "UNKNOWN";
}