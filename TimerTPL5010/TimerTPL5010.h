#ifndef _TIMERTPL5010_H
#define _TIMERTPL5010_H

#include "mbed.h"

class TimerTPL5010 {

public:
  TimerTPL5010(PinName pin_wake, PinName pin_done);

  void init(EventQueue *evq);
  bool get_wdt();
  void set_wdt(bool temp);

private:
  EventQueue *queue;
  Mutex mutex;

  InterruptIn pwake;
  DigitalOut pdone;

  volatile bool bwake;

  void kick_wdt();
  void fall_wake();
};

#endif