#include "engine/ClueReader.hpp"
#include "engine/Random.hpp"
#include "engine/ResourceFS.hpp"
#include <vector>

using namespace rapidxml;

// Parse the XML file into rapidxml document nodes.
void ClueReader::readFile(const std::string& filename) {
    // Route through ResourceFS so disk-backed dev builds and the
    // standalone embedded build both work without modifying call sites.
    // The bytes are stored on the instance because rapidxml's parse<0>
    // keeps pointers into them.
    std::vector<char> bytes;
    if (hh::ResourceFS::readAll(filename, bytes)) {
        xmlContent.assign(bytes.begin(), bytes.end());
    } else {
        xmlContent.clear();
    }
    if (xmlContent.empty()) {
        // rapidxml's parse<0> expects a writable null-terminated buffer.
        // An empty document is still valid input; just produce an empty
        // string so doc.parse() has a NUL to anchor on.
        xmlContent.assign(1, '\0');
    }
    doc.parse<0>(&xmlContent[0]);
}

// Returns the number of children of the given xml node.
int ClueReader::getNumChild(xml_node<> *parent) {
    int count = 0;
    for (xml_node<> *child = parent->first_node(); child; child = child->next_sibling()) {
        count++;
    }
    return count;
}

// Picks a random child node at `index` under the given parent.
static xml_node<>* pickChildAt(xml_node<> *parent, int index) {
    xml_node<> *node = parent->first_node();
    for (int n = 0; n < index && node; n++) {
        node = node->next_sibling();
    }
    return node;
}

// Pushes a clue from `node->first_node(category)` into `out` if present.
static void pushClue(xml_node<> *clues, const char* category, std::vector<std::string>& out) {
    if (!clues) return;
    if (auto* c = clues->first_node(category)) {
        out.push_back(c->value());
    }
}

// Randomly selects a high-damage and a low-damage item, then populates the
// clue and info lists based on those items.
void ClueReader::selectItems() {
    SelectStream(1);
    xml_node<> *root = doc.first_node();

    cluesJackpot.clear();
    cluesSpec.clear();
    cluesVague.clear();
    cluesWorthless.clear();

    // High damage item
    int randH = Equilikely(0, getNumChild(root->first_node("high")) - 1);
    xml_node<> *itemH = pickChildAt(root->first_node("high"), randH);
    itemHigh.name = itemH->first_node("name")->value();
    itemHigh.type = itemH->first_node("type")->value();

    if (auto* clueNode = itemH->first_node("clues")) {
        pushClue(clueNode, "jackpot",   cluesJackpot);
        pushClue(clueNode, "specific",  cluesSpec);
        pushClue(clueNode, "vague",     cluesVague);
        pushClue(clueNode, "worthless", cluesWorthless);
    }

    // Low damage item
    int randL = Equilikely(0, getNumChild(root->first_node("low")) - 1);
    xml_node<> *itemL = pickChildAt(root->first_node("low"), randL);
    itemLow.name = itemL->first_node("name")->value();
    itemLow.type = itemL->first_node("type")->value();

    auto* lowClues = itemL->first_node("clues");
    cluesJackpot.push_back(lowClues->first_node("jackpot")->value());
    cluesSpec.push_back(lowClues->first_node("specific")->value());
    cluesVague.push_back(lowClues->first_node("vague")->value());
    cluesWorthless.push_back(lowClues->first_node("worthless")->value());

    // Populate info by item type(s)
    auto appendInfoFor = [&](const char* type) {
        xml_node<> *list = root->first_node("info")->first_node(type);
        if (!list) return;
        for (xml_node<> *node = list->first_node(); node; node = node->next_sibling()) {
            info.push_back(node->value());
        }
    };
    appendInfoFor(itemHigh.type.c_str());
    if (itemHigh.type != itemLow.type) {
        appendInfoFor(itemLow.type.c_str());
    }
}

const std::vector<std::string>& ClueReader::getInfo()           const { return info; }
const std::vector<std::string>& ClueReader::getCluesJackpot()   const { return cluesJackpot; }
const std::vector<std::string>& ClueReader::getCluesSpec()      const { return cluesSpec; }
const std::vector<std::string>& ClueReader::getCluesVague()     const { return cluesVague; }
const std::vector<std::string>& ClueReader::getCluesWorthless() const { return cluesWorthless; }
const Item& ClueReader::getItemHigh() const { return itemHigh; }
const Item& ClueReader::getItemLow()  const { return itemLow; }
