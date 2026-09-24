#pragma once
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>
#include <utility>

namespace foocrate::devices {
// Device browsing never changes playback or ordinary playlist selection.
struct Context {
    bool active{};
    int tab{1};
    int previousTab{1};
    void enter() noexcept { active = true; }
    void choose(int next) noexcept {
        if (next < 0 || next > 2 || (next == 2 && !active)) return;
        if (next == 2 && tab != 2) previousTab = tab;
        tab = next;
    }
    void cycle(bool lyricsAvailable) noexcept {
        int next = (tab + 1) % (active ? 3 : 2);
        if (next == 0 && !lyricsAvailable) next = 1;
        choose(next);
    }
    void leave() noexcept { active = false; if (tab == 2) tab = previousTab; }
    bool overview() const noexcept { return active && tab == 2; }
};
struct Track { std::uint32_t id{}; std::wstring title, artist, album; std::string relativePath; };
class LibraryIndex {
public:
    bool append(Track track) {
        if (!index_.emplace(track.id, tracks.size()).second) return false;
        tracks.push_back(std::move(track)); return true;
    }
    std::optional<std::size_t> find(std::uint32_t id) const {
        const auto found = index_.find(id);
        if (found == index_.end()) return {};
        return found->second;
    }
    void clear() { tracks.clear(); index_.clear(); }
    std::vector<Track> tracks;
private:
    std::unordered_map<std::uint32_t, std::size_t> index_;
};
}
