#include <naturalehia/fauna/engine.hpp>

int main() {
    const naturalehia::fauna::TrackingEngine engine;
    return engine.tracks().empty() ? 0 : 1;
}
