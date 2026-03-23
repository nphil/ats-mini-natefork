#ifndef BUTTON_H
#define BUTTON_H

#define DEBOUNCE_INTERVAL    50
#define SHORT_PRESS_INTERVAL 1000 // Hold 1s then release → volume menu
#define LONG_PRESS_INTERVAL  2000
#define HOLD_SLEEP_MS        3000  // Hold duration (ms) to trigger sleep via hold gesture

class ButtonTracker {
  public:
    struct State {
      bool isPressed;       // Current pressed state (after debounce)
      bool wasClicked;      // Released after <1s press
      bool wasShortPressed; // Released after 1s–2s press → opens volume
      bool isLongPressed;   // Still pressed after >2s
    };

  ButtonTracker();
  void reset();
  State update(bool currentState, unsigned int debounceInterval = DEBOUNCE_INTERVAL);
  unsigned long getPressedDuration() const;

  private:
    bool lastState;
    bool lastStableState;
    unsigned long lastDebounceTime;
    unsigned long pressStartTime;
};

#endif
