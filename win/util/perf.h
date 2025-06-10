#pragma once
#include <chrono>

namespace hydra::util {

    class perf {
    public:
        perf() {
            restart();
        }

        void restart() {
            start = std::chrono::high_resolution_clock::now();
        }

        double elapsed_ms() const {
            auto diff = std::chrono::high_resolution_clock::now() - start;

            return std::chrono::duration_cast<std::chrono::milliseconds>(diff).count();
        }

    private:
        std::chrono::time_point<std::chrono::high_resolution_clock> start;
    };
}