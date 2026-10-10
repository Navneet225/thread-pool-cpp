// Generic thread pool (C++17): variadic template submit() returning std::future.
// Build: g++ -O2 -std=c++17 -pthread thread_pool.cpp -o thread_pool
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <string>
#include <thread>
#include <type_traits>
#include <vector>

class ThreadPool {
public:
    explicit ThreadPool(size_t n) {
        if (n == 0) throw std::invalid_argument("thread count must be > 0");
        workers_.reserve(n);
        for (size_t i = 0; i < n; ++i)
            workers_.emplace_back([this] { worker_loop(); });
    }

    // Destructor drains all queued tasks, then joins the workers.
    ~ThreadPool() {
        {
            std::lock_guard<std::mutex> lk(m_);
            stop_ = true;
        }
        cv_.notify_all();
        for (auto& t : workers_) t.join();
    }

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    // Submit any callable with any arguments; get the result (or exception) via future.
    template <typename F, typename... Args>
    auto submit(F&& f, Args&&... args) -> std::future<std::invoke_result_t<F, Args...>> {
        using R = std::invoke_result_t<F, Args...>;
        auto task = std::make_shared<std::packaged_task<R()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...));
        std::future<R> fut = task->get_future();
        {
            std::lock_guard<std::mutex> lk(m_);
            if (stop_) throw std::runtime_error("submit on stopped ThreadPool");
            q_.emplace([task] { (*task)(); });
        }
        cv_.notify_one();
        return fut;
    }

private:
    void worker_loop() {
        for (;;) {
            std::function<void()> job;
            {
                std::unique_lock<std::mutex> lk(m_);
                cv_.wait(lk, [this] { return stop_ || !q_.empty(); });
                if (stop_ && q_.empty()) return;
                job = std::move(q_.front());
                q_.pop();
            }
            job();
        }
    }

    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> q_;
    std::mutex m_;
    std::condition_variable cv_;
    bool stop_ = false;
};

static int check_count = 0, fail_count = 0;
#define CHECK(cond, msg) do { ++check_count; if (!(cond)) { ++fail_count; std::printf("FAIL: %s\n", msg); } } while (0)

int main() {
    // 1) Many tasks, results summed and verified.
    {
        ThreadPool pool(4);
        constexpr int N = 100000;
        std::vector<std::future<long long>> futs;
        futs.reserve(N);
        auto t0 = std::chrono::steady_clock::now();
        for (int i = 1; i <= N; ++i)
            futs.push_back(pool.submit([](long long x) { return x * x; }, (long long)i));
        long long sum = 0;
        for (auto& f : futs) sum += f.get();
        double s = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        long long expected = (long long)N * (N + 1) * (2LL * N + 1) / 6;
        CHECK(sum == expected, "sum of squares mismatch");
        std::printf("100000 tasks on 4 threads: %.3f s (%.0f tasks/s), sum %s\n",
                    s, N / s, sum == expected ? "OK" : "WRONG");
    }
    // 2) Different return types through the same template.
    {
        ThreadPool pool(2);
        auto a = pool.submit([](int x, int y) { return x + y; }, 2, 3);
        auto b = pool.submit([](const std::string& s) { return s + "!"; }, std::string("hello"));
        CHECK(a.get() == 5, "int result");
        CHECK(b.get() == "hello!", "string result");
    }
    // 3) Exceptions propagate through the future.
    {
        ThreadPool pool(2);
        auto f = pool.submit([]() -> int { throw std::runtime_error("boom"); });
        bool caught = false;
        try { f.get(); } catch (const std::runtime_error& e) { caught = std::string(e.what()) == "boom"; }
        CHECK(caught, "exception propagation");
    }
    // 4) Destructor drains queued tasks before shutdown.
    {
        std::atomic<int> done{0};
        {
            ThreadPool pool(2);
            for (int i = 0; i < 1000; ++i) pool.submit([&done] { ++done; });
        }
        CHECK(done == 1000, "queue drained on destruction");
    }
    std::printf("%d checks, %d failed\n", check_count, fail_count);
    return fail_count == 0 ? 0 : 1;
}
