#pragma once

// Turns raw touch samples into taps and swipes. Pure logic, no hardware.

#include <stdint.h>
#include <stdlib.h>

enum class GestureType { None, Tap, SwipeLeft, SwipeRight, SwipeUp, SwipeDown };

struct Gesture {
  GestureType type = GestureType::None;
  int x = 0, y = 0; // where the finger first touched
};

class GestureDetector {
public:
  // Feed one sample. `down` is whether a finger is on the screen.
  Gesture update(bool down, int x, int y, uint32_t nowMs) {
    Gesture g;
    if (down) {
      if (!_active) {
        _active = true;
        _startX = _lastX = _farX = x;
        _startY = _lastY = _farY = y;
        _startMs = nowMs;
      } else {
        _lastX = x;
        _lastY = y;
        const int d2 = (x - _startX) * (x - _startX) + (y - _startY) * (y - _startY);
        const int f2 = (_farX - _startX) * (_farX - _startX) + (_farY - _startY) * (_farY - _startY);
        if (d2 > f2) {
          _farX = x;
          _farY = y;
        }
      }
      _lastDownMs = nowMs;
      return g;
    }
    // Finger lifted (allow brief dropouts between samples).
    if (!_active || nowMs - _lastDownMs < kReleaseMs) return g;
    _active = false;

    // Use the farthest point reached, not just the last sample: a finger
    // often drifts back a little as it lifts off at the end of a flick.
    const int dx = _farX - _startX;
    const int dy = _farY - _startY;
    g.x = _startX;
    g.y = _startY;
    if (abs(dx) >= kSwipeMin && abs(dx) * 4 >= abs(dy) * 3) {
      // Mostly horizontal (within ~53 degrees of the x axis).
      g.type = dx > 0 ? GestureType::SwipeRight : GestureType::SwipeLeft;
    } else if (abs(dy) >= kSwipeMin) {
      g.type = dy > 0 ? GestureType::SwipeDown : GestureType::SwipeUp;
    } else if (abs(dx) < kTapSlop && abs(dy) < kTapSlop &&
               _lastDownMs - _startMs < kTapMaxMs) {
      g.type = GestureType::Tap;
    }
    return g;
  }

  bool active() const { return _active; }

private:
  static constexpr int kSwipeMin = 45;   // px
  static constexpr int kTapSlop = 24;    // px
  static constexpr uint32_t kTapMaxMs = 700;
  // The touch chip sometimes misses the finger for a sample or two; only a
  // gap longer than this counts as lifting the finger.
  static constexpr uint32_t kReleaseMs = 70;

  bool _active = false;
  int _startX = 0, _startY = 0, _lastX = 0, _lastY = 0, _farX = 0, _farY = 0;
  uint32_t _startMs = 0, _lastDownMs = 0;
};
