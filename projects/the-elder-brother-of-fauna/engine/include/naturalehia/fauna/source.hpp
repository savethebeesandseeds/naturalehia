#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

#include <naturalehia/fauna/model.hpp>

namespace naturalehia::fauna {

enum class ReadStatus : std::uint8_t {
    data,
    would_block,
    end,
    error,
};

struct ReadResult {
    std::size_t count{};
    ReadStatus status{ReadStatus::would_block};
    std::string message{};
};

class ObservationSource {
  public:
    virtual ~ObservationSource() = default;

    [[nodiscard]] virtual SourceId id() const noexcept = 0;

    // Fills at most output.size() entries. Sequences must increase and timestamps must not
    // move backwards. Within one timestamp, emit stable-tag observations before untagged
    // detections when the group may be split across reads; hard identity evidence is reserved
    // before proximity association.
    virtual ReadResult read(std::span<Observation> output) = 0;
};

} // namespace naturalehia::fauna
