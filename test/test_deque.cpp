#include <iostream>
#include <thread>
#include <vector>
#include <chrono>
#include <optional>
#include "../LockFreeDeque.h"

int main() {
    LockFreeDeque<int> dq(8);

    // Owner thread will push numbers 1..N then try to pop them.
    const int N = 1000;
    std::thread owner([&dq]() {
        for (int i = 1; i <= N; ++i) {
            dq.push_bottom(i);
        }

        // owner pops some items (simulate doing work)
        for (int i = 0; i < 200; ++i) {
            auto v = dq.pop_bottom();
            if (v) {
                // process
                // std::cout << "owner popped " << *v << "\n";
            } else {
                // nothing
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    });

    // multiple thieves trying to steal concurrently
    const int thieves = 4;
    std::vector<std::thread> workers;
    std::atomic<int> total_stolen{0};
    for (int t = 0; t < thieves; ++t) {
        workers.emplace_back([&dq, &total_stolen]() {
            while (true) {
                auto v = dq.steal_top();
                if (v) {
                    ++total_stolen;
                    // std::cout << "stole " << *v << "\n";
                } else {
                    // If deque might still be filling, wait a bit; otherwise break.
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    // crude termination condition for demo
                    if (dq.size_approx() == 0) {
                        break;
                    }
                }
            }
        });
    }

    owner.join();
    // give thieves a bit to finish stealing
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    for (auto &w : workers) w.join();

    std::cout << "approx remaining size: " << dq.size_approx() << "\n";
    std::cout << "total stolen (approx): " << total_stolen.load() << "\n";
    return 0;
}
