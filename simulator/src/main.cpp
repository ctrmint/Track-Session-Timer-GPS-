#include "device_screen.hpp"

#include "track_timer/simulator/scenario.hpp"
#include "track_timer/ui/presenter.hpp"

#include <SDL2/SDL.h>
#include <lvgl.h>

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>

namespace {

constexpr std::int32_t kDisplayWidth = 600;
constexpr std::int32_t kDisplayHeight = 450;

struct Options {
    track_timer::simulator::ScenarioId scenario{track_timer::simulator::ScenarioId::active};
    track_timer::simulator::GnssReplayRate gnss_rate{
        track_timer::simulator::GnssReplayRate::hz25};
    bool headless{false};
    std::size_t frames{25};
    std::int64_t frame_ms{40};
    std::string snapshot_path{};
    std::string gnss_fixture_path{};
};

std::string require_value(const int argc, char** argv, int& index)
{
    if (index + 1 >= argc) {
        throw std::invalid_argument(std::string("missing value for ") + argv[index]);
    }
    return argv[++index];
}

std::size_t parse_positive_size(const std::string& value, const char* option)
{
    const auto parsed = std::stoull(value);
    if (parsed == 0) {
        throw std::invalid_argument(std::string(option) + " must be greater than zero");
    }
    return static_cast<std::size_t>(parsed);
}

Options parse_options(const int argc, char** argv)
{
    Options options{};
    for (int index = 1; index < argc; ++index) {
        const std::string argument{argv[index]};
        if (argument == "--headless") {
            options.headless = true;
        }
        else if (argument == "--scenario") {
            const auto name = require_value(argc, argv, index);
            if (!track_timer::simulator::parse_scenario(name, options.scenario)) {
                throw std::invalid_argument("unknown scenario: " + name);
            }
        }
        else if (argument == "--frames") {
            options.frames = parse_positive_size(require_value(argc, argv, index), "--frames");
        }
        else if (argument == "--frame-ms") {
            options.frame_ms = static_cast<std::int64_t>(
                parse_positive_size(require_value(argc, argv, index), "--frame-ms"));
        }
        else if (argument == "--gnss-rate") {
            const auto value = require_value(argc, argv, index);
            if (value == "20") {
                options.gnss_rate = track_timer::simulator::GnssReplayRate::hz20;
            }
            else if (value == "25") {
                options.gnss_rate = track_timer::simulator::GnssReplayRate::hz25;
            }
            else {
                throw std::invalid_argument("--gnss-rate must be 20 or 25");
            }
        }
        else if (argument == "--gnss-fixture") {
            options.gnss_fixture_path = require_value(argc, argv, index);
        }
        else if (argument == "--snapshot") {
            options.snapshot_path = require_value(argc, argv, index);
        }
        else if (argument == "--help") {
            std::cout << "Usage: track_timer_simulator [--scenario NAME] [--headless] "
                         "[--frames COUNT] [--frame-ms MS] [--gnss-rate 20|25] "
                         "[--gnss-fixture FILE] [--snapshot FILE]\n"
                         "Scenarios: ready, active, gnss-loss, storage-failure\n"
                         "Click the interactive screen to cycle scenarios.\n";
            std::exit(0);
        }
        else {
            throw std::invalid_argument("unknown option: " + argument);
        }
    }
    return options;
}

bool write_snapshot(const std::string& path)
{
    auto* snapshot = lv_snapshot_take(lv_screen_active(), LV_COLOR_FORMAT_ARGB8888);
    if (snapshot == nullptr || snapshot->header.w != kDisplayWidth ||
        snapshot->header.h != kDisplayHeight) {
        if (snapshot != nullptr) {
            lv_draw_buf_destroy(snapshot);
        }
        return false;
    }

    std::ofstream output(path, std::ios::binary);
    if (!output) {
        lv_draw_buf_destroy(snapshot);
        return false;
    }

    output << "P6\n" << snapshot->header.w << ' ' << snapshot->header.h << "\n255\n";
    for (std::uint32_t y = 0; y < snapshot->header.h; ++y) {
        const auto* row = snapshot->data + y * snapshot->header.stride;
        for (std::uint32_t x = 0; x < snapshot->header.w; ++x) {
            const auto* pixel = reinterpret_cast<const lv_color32_t*>(row + x * sizeof(lv_color32_t));
            output.put(static_cast<char>(pixel->red));
            output.put(static_cast<char>(pixel->green));
            output.put(static_cast<char>(pixel->blue));
        }
    }
    const bool succeeded = static_cast<bool>(output);
    lv_draw_buf_destroy(snapshot);
    return succeeded;
}

struct ApplicationContext {
    track_timer::simulator::ScenarioPlayer player;
    track_timer::simulator::DeviceScreen* screen;

    ApplicationContext(const track_timer::simulator::ScenarioId scenario,
                       track_timer::simulator::GnssFixture fixture,
                       const track_timer::simulator::GnssReplayRate rate,
                       track_timer::simulator::DeviceScreen* device_screen)
        : player(scenario, std::move(fixture), rate), screen(device_screen)
    {
    }
};

void update_screen(ApplicationContext& context)
{
    context.screen->update(track_timer::ui::present(context.player.snapshot()));
}

void cycle_scenario(lv_event_t* event)
{
    auto* context = static_cast<ApplicationContext*>(lv_event_get_user_data(event));
    context->player.reset(track_timer::simulator::next_scenario(context->player.id()));
    lv_point_t point{};
    if (auto* input = lv_indev_active(); input != nullptr) {
        lv_indev_get_point(input, &point);
    }
    context->player.device().touch().inject(static_cast<std::int16_t>(point.x),
                                             static_cast<std::int16_t>(point.y), true);
    update_screen(*context);
}

int run(const Options& options)
{
    if (options.headless && std::getenv("SDL_VIDEODRIVER") == nullptr) {
        SDL_setenv("SDL_VIDEODRIVER", "dummy", 1);
    }

    lv_init();
    auto* display = lv_sdl_window_create(kDisplayWidth, kDisplayHeight);
    if (display == nullptr) {
        std::cerr << "Failed to create the 600 x 450 SDL display\n";
        lv_deinit();
        return 1;
    }
    lv_sdl_window_set_title(display, "Track Session Timer — device simulator");
    lv_sdl_window_set_resizeable(display, false);

    auto* mouse = lv_sdl_mouse_create();
    lv_indev_set_display(mouse, display);

    auto fixture = track_timer::simulator::make_synthetic_gnss_fixture();
    if (!options.gnss_fixture_path.empty()) {
        std::string error;
        if (!track_timer::simulator::load_gnss_fixture(options.gnss_fixture_path, fixture, error)) {
            throw std::invalid_argument(error);
        }
    }

    track_timer::simulator::DeviceScreen screen{lv_screen_active()};
    ApplicationContext context{options.scenario, std::move(fixture), options.gnss_rate, &screen};
    update_screen(context);
    lv_obj_add_event_cb(lv_screen_active(), cycle_scenario, LV_EVENT_CLICKED, &context);
    lv_refr_now(display);

    if (options.headless) {
        for (std::size_t frame = 0; frame < options.frames; ++frame) {
            context.player.advance(options.frame_ms);
            update_screen(context);
            lv_timer_handler();
            lv_refr_now(display);
        }
    }
    else {
        auto previous_tick = SDL_GetTicks64();
        while (lv_display_get_next(nullptr) != nullptr) {
            const auto current_tick = SDL_GetTicks64();
            if (current_tick - previous_tick >= static_cast<std::uint64_t>(options.frame_ms)) {
                context.player.advance(options.frame_ms);
                update_screen(context);
                previous_tick = current_tick;
            }
            lv_timer_handler();
            SDL_Delay(5);
        }
    }

    if (!options.snapshot_path.empty() && !write_snapshot(options.snapshot_path)) {
        std::cerr << "Failed to write snapshot: " << options.snapshot_path << '\n';
        lv_display_delete(display);
        lv_sdl_quit();
        lv_deinit();
        return 1;
    }

    const auto model = track_timer::ui::present(context.player.snapshot());
    const auto diagnostics = context.player.diagnostics();
    const auto& gnss = context.player.device().gnss();
    const auto& storage = context.player.device().storage();
    std::cout << "scenario=" << track_timer::simulator::scenario_name(context.player.id())
              << " frames=" << options.frames << " resolution=" << kDisplayWidth << 'x'
              << kDisplayHeight << " current=" << model.current_lap.data()
              << " session=" << model.session_remaining.data() << " gnss="
              << model.gnss_status.data() << " logging=" << model.logging_status.data()
              << " fixture=" << gnss.fixture_name()
              << " gnss-rate=" << static_cast<unsigned>(gnss.rate())
              << " gnss-mode=" << track_timer::simulator::gnss_mode_name(gnss.mode())
              << " gnss-dropped=" << diagnostics.gnss.queue.dropped
              << " storage-mode=" << track_timer::simulator::storage_mode_name(storage.mode())
              << " storage-failures=" << storage.status().write_failures
              << " storage-recoveries=" << diagnostics.storage.recoveries << '\n';

    if (lv_display_get_next(nullptr) != nullptr) {
        lv_display_delete(display);
    }
    lv_sdl_quit();
    lv_deinit();
    return 0;
}

}  // namespace

int main(const int argc, char** argv)
{
    try {
        return run(parse_options(argc, argv));
    }
    catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 2;
    }
}
