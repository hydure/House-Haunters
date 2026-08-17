#ifndef INVESTIGATION_JOURNAL_HPP
#define INVESTIGATION_JOURNAL_HPP

#include <string>
#include <vector>

class InvestigationJournal
{
public:
    enum class Tier {
        WORTHLESS,
        VAGUE,
        SPECIFIC,
        JACKPOT
    };

    struct Entry {
        std::string text;
        Tier tier = Tier::WORTHLESS;
        int discoveredBy = -1;
    };

    static InvestigationJournal& instance();

    void reset();
    bool discover(const std::string& text, Tier tier, int discoveredBy);
    const std::vector<Entry>& entries() const { return entries_; }

    static const char* tierName(Tier tier);

private:
    std::vector<Entry> entries_;
};

#endif