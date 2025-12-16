#include <benchmark/benchmark.h>

#include <libsbx/memory/local_containers.hpp>

static auto benchmark_local_unordered_map_try_emplace(benchmark::State& state) -> void {
  constexpr std::size_t Capacity = 2048;

  for (auto _ : state) {
    state.PauseTiming();

    auto map = sbx::memory::local_unordered_map<int, int, Capacity>{};
    auto n = state.range(0);

    state.ResumeTiming();

    for (auto i = 0; i < n; ++i) {
      map.try_emplace(i, i * 2);
    }

    benchmark::ClobberMemory();
  }
}

BENCHMARK(benchmark_local_unordered_map_try_emplace)->Range(64, 2048);

static auto benchmark_std_unordered_map_try_emplace(benchmark::State& state) -> void {
  for (auto _ : state) {
    state.PauseTiming();

    auto map = std::unordered_map<int, int>{};
    auto n = state.range(0);

    state.ResumeTiming();

    for (auto i = 0; i < n; ++i) {
      map.try_emplace(i, i * 2);
    }

    benchmark::ClobberMemory();
  }
}

BENCHMARK(benchmark_std_unordered_map_try_emplace)->Range(64, 2048);

static auto benchmark_local_vector_push_back(benchmark::State& state) -> void {
  constexpr std::size_t Capacity = 2048;

  for (auto _ : state) {
    state.PauseTiming();

    auto vec = sbx::memory::local_vector<int, Capacity>{};
    auto n = state.range(0);

    state.ResumeTiming();

    for (auto i = 0; i < n; ++i) {
      vec.push_back(i);
    }

    benchmark::ClobberMemory();
  }
}

BENCHMARK(benchmark_local_vector_push_back)->Range(64, 2048);

static auto benchmark_std_vector_push_back(benchmark::State& state) -> void {
  for (auto _ : state) {
    state.PauseTiming();

    auto vec = std::vector<int>{};
    auto n = state.range(0);

    state.ResumeTiming();

    for (auto i = 0; i < n; ++i) {
      vec.push_back(i);
    }

    benchmark::ClobberMemory();
  }
}

BENCHMARK(benchmark_std_vector_push_back)->Range(64, 2048);

BENCHMARK_MAIN();
