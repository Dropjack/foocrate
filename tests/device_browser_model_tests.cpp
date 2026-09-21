#include "device_browser_model.h"
#include <iostream>
#include <stdexcept>
using namespace foocrate::devices;
void check(bool value) { if (!value) throw std::runtime_error("Device browser invariant failed"); }
int main() {
    try {
        Context c; c.choose(0); c.enter(); check(c.active && c.tab == 0 && !c.overview());
        c.choose(2); check(c.overview()); c.leave(); check(!c.active && c.tab == 0);
        c.choose(2); check(c.tab == 0); // no global third tab
        c.enter(); c.choose(1); c.choose(2); c.leave(); check(c.tab == 1);
        c.enter(); c.choose(2); c.enter(); check(c.overview()); // switching devices keeps explicit tab
        c.choose(0); c.cycle(true); check(c.tab == 1);
        c.cycle(true); check(c.overview());
        c.cycle(true); check(c.tab == 0); // three pages, no visible tabs
        c.choose(1); c.cycle(false); check(c.overview());
        c.cycle(false); check(c.tab == 1); // missing lyrics is skipped
        c.cycle(false); c.leave(); check(!c.active && c.tab == 1);
        c.cycle(true); check(c.tab == 0); c.cycle(true); check(c.tab == 1);
        LibraryIndex library;
        check(library.append({42,L"A",L"Artist",L"Album"}));
        check(library.append({99,L"B",L"Artist",L"Album"}));
        check(!library.append({42,L"Duplicate",L"",L""}));
        std::vector<std::size_t> members;
        for (auto id : {99U,42U,99U}) { auto row=library.find(id); check(row.has_value()); members.push_back(*row); }
        check(members == std::vector<std::size_t>({1,0,1})); // preserve stored playlist order/repeats
        check(!library.find(100)); library.clear(); check(!library.find(42) && library.tracks.empty());
        std::cout << "Conditional tabs, restoration and device playlist ID mapping passed.\n";
        return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
