# Generic Thread Pool (C++17)

A fixed-size thread pool with a variadic-template `submit()` that accepts any callable and arguments and returns a `std::future` for the result.

## Features
- `submit(f, args...)` is a template: works with lambdas, functions and functors, with any return type
- Results and exceptions are delivered through `std::future` (`get()` rethrows the task's exception)
- Mutex + condition-variable work queue; idle workers sleep instead of spinning
- RAII shutdown: the destructor lets workers finish all queued tasks, then joins them
- Non-copyable; `submit` after shutdown throws

## Usage
```cpp
ThreadPool pool(4);
auto a = pool.submit([](int x, int y) { return x + y; }, 2, 3);
auto b = pool.submit([](const std::string& s) { return s + "!"; }, std::string("hello"));
std::cout << a.get() << " " << b.get();   // 5 hello!
```

## Build and run
Requirements: Linux, g++ with C++17. On Windows, use WSL.

```bash
make
./thread_pool
```

Sample output:
```
100000 tasks on 4 threads: 0.154 s (649470 tasks/s), sum OK
5 checks, 0 failed
```
(Timing is from a single-core sandbox, so treat it as indicative only.)

## Tests (in `main`)
1. 100,000 tasks computing squares; the sum of results is checked against the closed-form value
2. Different return types (`int`, `std::string`) through the same template
3. Exception thrown inside a task is caught by the caller through the future
4. 1,000 queued tasks all complete before the pool is destroyed

## Thread safety check
```bash
make tsan && ./thread_pool_tsan
```
Running the tests under ThreadSanitizer with the exception test removed gave 0 warnings. With the exception test included, ThreadSanitizer reports races inside libstdc++'s `exception_ptr` / exception destructor code, which is not instrumented. I have not investigated this further, so treat it as a likely tooling limitation rather than a confirmed result.

## Limitations
- No task priorities, work stealing, or dynamic resizing
- A single shared queue and lock, so very fine-grained tasks contend on the mutex
- `std::function` and `std::shared_ptr` allocate per task

## Author
Navneet Singh Yadav - [LinkedIn](https://www.linkedin.com/in/navneet-singh-yadav-439886281/)
