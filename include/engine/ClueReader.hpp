#ifndef CLUEREADER_HPP
#define CLUEREADER_HPP
#include "rapidxml/rapidxml.hpp"
#include "rapidxml/rapidxml_utils.hpp"
#include <string>
#include <vector>

struct Item {
    std::string name;
    std::string type;
};

class ClueReader {
public:
    void readFile(const std::string& filename);
    void selectItems();
    const std::vector<std::string>& getInfo() const;
    const std::vector<std::string>& getCluesJackpot() const;
    const std::vector<std::string>& getCluesSpec() const;
    const std::vector<std::string>& getCluesVague() const;
    const std::vector<std::string>& getCluesWorthless() const;
    const Item& getItemHigh() const;
    const Item& getItemLow() const;

private:
    int getNumChild(rapidxml::xml_node<> *parent);
    Item itemHigh;
    Item itemLow;
    std::vector<std::string> info;
    std::vector<std::string> cluesJackpot;
    std::vector<std::string> cluesSpec;
    std::vector<std::string> cluesVague;
    std::vector<std::string> cluesWorthless;
    rapidxml::xml_document<> doc;
    // rapidxml::xml_document::parse<0> is non-owning -- it stores pointers
    // into the source buffer. Keep that buffer alive for the lifetime of
    // this reader so subsequent traversals don't dereference freed memory.
    std::string xmlContent;
};

#endif
