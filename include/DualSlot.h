#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>

// Caller supplies synchronization. Fixed storage, FIFO reliable + latest high rate.
class DualSlot {
 public:
  struct Item { char json[256]{}; uint32_t received=0; };
  bool push(const char* json, uint32_t now, bool reliable) {
    if (!json || std::strlen(json)>255) return false;
    Item item; std::strcpy(item.json,json); item.received=now;
    if (!reliable) { fast_=item; fastPending_=true; return true; }
    if (count_==8) return false;
    queue_[(head_+count_)%8]=item; ++count_; return true;
  }
  bool pop(Item& item) {
    if (count_ && (!fastPending_ || preferReliable_)) {
      item=queue_[head_]; head_=(head_+1)%8; --count_; preferReliable_=false; return true;
    }
    if (fastPending_) { item=fast_; fastPending_=false; preferReliable_=true; return true; }
    return false;
  }
  void clear() { head_=count_=0; fastPending_=false; preferReliable_=true; }
 private:
  Item fast_,queue_[8]; size_t head_=0,count_=0;
  bool fastPending_=false,preferReliable_=true;
};
