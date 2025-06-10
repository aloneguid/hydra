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
        std::vector<ble_central> centrals;
        bool centrals_dirty{false};

        std::string error_message;
        bool is_running{true};
        void run_frame();
        void render_toolbar();
        void render_centrals();

        // emulator
        std::string emu_text;
        float emu_mouse_speed{1.0f};
        void render_emulator();
    };
}