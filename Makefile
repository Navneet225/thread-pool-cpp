CXX ?= g++
CXXFLAGS ?= -O2 -std=c++17 -Wall -Wextra

thread_pool: thread_pool.cpp
	$(CXX) $(CXXFLAGS) -pthread $< -o $@

# Build with ThreadSanitizer to check for data races
tsan: thread_pool.cpp
	$(CXX) -O1 -g -std=c++17 -fsanitize=thread -pthread $< -o thread_pool_tsan

clean:
	rm -f thread_pool thread_pool_tsan
