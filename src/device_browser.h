#pragma once
#include <windows.h>
#include <functional>
#include <memory>
#include <string>
#include <cstdint>
#include "theme_model.h"

namespace foocrate {
class DeviceBrowser final {
public:
    DeviceBrowser();
    ~DeviceBrowser();
    void create(HWND parent, std::function<void(int)> changed);
    void destroy();
    bool available() const;
    bool active() const;
    bool overview() const;
    float sidebarBottom(float height, float minimum) const;
    void leave();
    void baseTab(int tab);
    void cycle(bool lyricsAvailable);
    void layout(RECT sidebar, RECT table, RECT lower, bool visible, UINT dpi, const ThemePalette& palette);
    void importTrack(const std::string& source, const std::string& title,
        const std::string& artist, const std::string& album,
        double durationSeconds, std::uint64_t sourceSize);
    void importTrackToFirstDevice(const std::string& source, const std::string& title,
        const std::string& artist, const std::string& album,
        double durationSeconds, std::uint64_t sourceSize);
    bool message(UINT msg, WPARAM wp, LPARAM lp, LRESULT& result);
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
DeviceBrowser* currentDeviceBrowser() noexcept;
bool importTrackToFirstDevice(const std::string& source, const std::string& title,
    const std::string& artist, const std::string& album, double durationSeconds,
    std::uint64_t sourceSize);
}
