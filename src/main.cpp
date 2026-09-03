#include "engine.hpp"
#include <common.hpp>

// TODO: this should be removed, as in the future the mde portion should be an
// library that's imported. Fine for early testing to iron out the API though.
int main() {

  spdlog::set_level(spdlog::level::trace);
  spdlog::info("Hello, World!");

  auto engine = mde::Engine(mde::IngestData::NYSE_PILLAR, "/tmp/");
  (void)engine;

  return 0lu;
}
