#include <f1tm/sim_runner.hpp>
#include <f1tm/viewer/app.hpp>

using namespace f1tm;

int main() {
  SimRunner sim;
  sim.configure_default_world();
  sim.start();

  ViewerApp app(sim);
  const int code = app.run();

  sim.stop();
  return code;
}
