#pragma once
#include "grey.h"
#include "../machine.h"

namespace hydra::ui {
    class control {
    public:
        control(hydra::machine& machine);
        ~control();

        void run();

    private:
        machine& machine;

        std::unique_ptr<grey::app> app;
        grey::widgets::window wnd_main;
        grey::widgets::container centrals_container;
        std::vector<ble_central> centrals;
        bool centrals_dirty{false};

        std::string error_message;
        bool is_running{true};
        void run_frame();
        void render_toolbar();
        void render_centrals();
        void render_status_bar();

        // emulator
        grey::widgets::window wnd_emu;
        bool wnd_emu_visible{false};
        std::string emu_text;
        int emu_mouse_speed{1};
        void render_emulator();
    };
}