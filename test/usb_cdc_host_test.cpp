// Run via test/run_usb_cdc_tests.py (MSVC). The runner includes the actual
// production submission/event functions with a simulated USB stream and queue.
#include <ArduinoJson.h>
#include "CommandLineBuffer.h"
#include "IdleDisplayPolicy.h"
#include <atomic>
#include <cassert>
#include <cstring>
#include <deque>
#include <iostream>
#include <string>

#define CMD_MAX_LEN 256
#define CMD_BREAK_LOOP 0
#include "DualSlot.h"
DualSlot commandSlots;
int commandMux;
#define portENTER_CRITICAL(x) ((void)0)
#define portEXIT_CRITICAL(x) ((void)0)
std::atomic<bool> ingressReady{true},breakloop{false};
struct Chassis { bool stopped=false; void requestStop(){stopped=true;} } chassis;
uint32_t millis(){return 100;}
bool pending(){DualSlot copy=commandSlots;DualSlot::Item i;return copy.pop(i);}
using esp_event_base_t = int;
enum { ARDUINO_USB_CDC_EVENTS, ARDUINO_USB_CDC_DISCONNECTED_EVENT,
       ARDUINO_USB_CDC_RX_OVERFLOW_EVENT, ARDUINO_USB_CDC_RX_EVENT };
struct Usb {
  std::deque<unsigned char> data;
  int available() { return static_cast<int>(data.size()); }
  int read() { if (data.empty()) return -1; int c = data.front(); data.pop_front(); return c; }
} USBSerial;
#include "usb_cdc_production.inc"

void event(int id) { usbCdcEvent(nullptr, ARDUINO_USB_CDC_EVENTS, id, nullptr); }
void feed(const std::string& s) {
  for (auto c : s) USBSerial.data.push_back(c);
  event(ARDUINO_USB_CDC_RX_EVENT);
}
int take() {
  DualSlot::Item item;assert(commandSlots.pop(item));
  JsonDocument doc;assert(!deserializeJson(doc,item.json));return doc["T"].as<int>();
}

int main() {
  feed("{\"T\":1}\n");
  assert(lastControlActivity.load() == millis());
  const int executing = take();
  feed("{\"T\":2}\n{\"T\":3}\n");
  assert(executing == 1 && take() == 3);
  feed("{\"T\":"); assert(!pending());
  feed("4}\r\n\n"); assert(take() == 4);
  feed(std::string(100000, 'x')); assert(!pending());
  feed("{\"T\":5}\n"); assert(!pending()); // oversized tail is discarded
  feed("{\"T\":6}\n"); assert(take() == 6);
  feed("{\"T\":7}\n");
  feed("broken\n{}\n{\"T\":\"0\"}\n"); assert(take() == 7 && !breakloop);
  feed("{\"T\":8}\n{ \"T\" : 0 }\n");
  assert(breakloop && !pending());
  feed("{\"T\":9}\n"); assert(take() == 9); // stop is not replayed later
  breakloop = false;
  feed("{\"T\":10,\"text\":\"\\\"T\\\":0\"}\n");
  assert(take() == 10 && !breakloop);
  feed("{\"T\":"); event(ARDUINO_USB_CDC_RX_OVERFLOW_EVENT);
  feed("11}\n{\"T\":12}\n"); assert(take() == 12);
  feed("{\"T\":"); event(ARDUINO_USB_CDC_DISCONNECTED_EVENT);
  feed("{\"T\":13}\n"); assert(take() == 13);
  feed(std::string("{\"T\":14}\0", 9) + "\n"); assert(!pending());
  std::string exact = "{\"T\":15}";
  exact.append(255 - exact.size(), ' ');
  feed(exact + "\n"); assert(take() == 15);
  feed(exact + " \n"); assert(!pending());
  // Exercise the short-literal producer and fixed-size zero-filled queue copy.
  addJsonCmd2Queue("{\"T\":16}"); assert(take() == 16);

  for (int i = 0; i < 100000; ++i) feed("{\"T\":17}\n{\"T\":18}\n");
  assert(take() == 18);
  feed("{\"T\":807,\"reliable\":true}\n");
  for(int i=0;i<10000;++i)feed("{\"T\":804}\n");
  int first=take(),second=take();assert((first==807&&second==804)||(first==804&&second==807));
  for(int i=0;i<8;++i)assert(submitJsonCommand("{\"T\":807,\"reliable\":true}"));
  assert(!submitJsonCommand("{\"T\":808,\"reliable\":true}"));
  feed("{\"T\":800}\n");assert(chassis.stopped&&!pending());
  assert(!submitJsonCommand("{\"T\":804,\"reliable\":1}"));
  std::cout << "PASS: dual-slot fairness/capacity/stop,  framing, latest-wins, stop, overflow recovery, 200000-command burst\n";
}
