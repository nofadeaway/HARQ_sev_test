/******************************************************************************
 *  File:         msg_queue.h
 *  Description:  Thread-safe bounded circular buffer of srsue_byte_buffer pointers.
 *  Reference:
 *****************************************************************************/

#ifndef MSG_QUEUE_H
#define MSG_QUEUE_H

#include "common/common.h"
#include <boost/thread/mutex.hpp>
#include <boost/thread/condition.hpp>

namespace srslte {

class msg_queue
{
public:
  msg_queue(uint32_t capacity_ = 128)
    :head(0)
    ,tail(0)
    ,unread(0)
    ,unread_bytes(0)
    ,capacity(capacity_)
  {
    buf = new byte_buffer_t*[capacity];
  }

  ~msg_queue()
  {
    delete [] buf;
  }

  void write(byte_buffer_t *msg)
  { 
    boost::mutex::scoped_lock lock(mutex); 
 
    while(is_full()) not_full.wait(lock);
    buf[head] = msg; 
    head = (head+1)%capacity;
    unread++; 
    unread_bytes += msg->N_bytes;
    lock.unlock();
    not_empty.notify_one();   //boost库中的条件变量通知
  }

  void read(byte_buffer_t **msg)
  { 
    boost::mutex::scoped_lock lock(mutex); 
    while(is_empty()) not_empty.wait(lock);  //printf("\n\nNow RLC READ!\n\n");
    *msg = buf[tail];
    tail = (tail+1)%capacity;
    unread--;
    unread_bytes -= (*msg)->N_bytes;
    lock.unlock();
    not_full.notify_one();
  }

  bool try_read(byte_buffer_t **msg)
  {
    boost::mutex::scoped_lock lock(mutex);
    if(is_empty())
    {
      return false;
    }else{
      *msg = buf[tail];
      tail = (tail+1)%capacity;
      unread--;
      unread_bytes -= (*msg)->N_bytes;
      lock.unlock();
      not_full.notify_one();
      return true;
    }
  }

  uint32_t size()
  {
    boost::mutex::scoped_lock lock(mutex);
    return unread;
  }

  uint32_t size_bytes()
  {
    boost::mutex::scoped_lock lock(mutex);
    return unread_bytes;
  }

  uint32_t size_tail_bytes()
  {
    boost::mutex::scoped_lock lock(mutex);
    return buf[tail]->N_bytes;
  }

private:
  bool     is_empty() const { return (unread == 0); }
  bool     is_full() const { return (unread == capacity); }

  boost::condition      not_empty;
  boost::condition      not_full;
  boost::mutex          mutex;
  byte_buffer_t **buf;
  uint32_t              capacity;
  uint32_t              unread;
  uint32_t              unread_bytes;
  uint32_t              head;
  uint32_t              tail;
};

} // namespace srsue


#endif // MSG_QUEUE_H
