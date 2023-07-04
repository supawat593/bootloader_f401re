#include "./TimerTPL5010/TimerTPL5010.h"

TimerTPL5010::TimerTPL5010(PinName pin_wake, PinName pin_done)
    : pwake(pin_wake), pdone(pin_done, 0) {

  kick_wdt();
}

void TimerTPL5010::init(EventQueue *evq) {
  bwake = false;
  queue = evq;

  pwake.fall(queue->event(this, &TimerTPL5010::fall_wake));
}

bool TimerTPL5010::get_wdt() {
  bool temp = false;
  mutex.lock();
  temp = bwake;
  mutex.unlock();
  return temp;
}

void TimerTPL5010::set_wdt(bool temp) {
  mutex.lock();
  bwake = temp;
  mutex.unlock();
}

void TimerTPL5010::kick_wdt() {
  pdone = 1;
  wait_us(10);
  pdone = 0;
}

void TimerTPL5010::fall_wake() {

  queue->call_in(250ms, this, &TimerTPL5010::kick_wdt);

  queue->call_in(500ms, printf, "<--------- kick_wdt() --------> : %p\r\n",
                 ThisThread::get_id());
  set_wdt(true);
}