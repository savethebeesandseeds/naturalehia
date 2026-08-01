#include <naturalehia/engine.hpp>

int main() {
    const naturalehia::TrackingEngine engine;
    return engine.tracks().empty() ? 0 : 1;
}
