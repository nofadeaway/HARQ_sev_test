/**
 *
 * \section COPYRIGHT
 *
 * Copyright 2013-2015 Software Radio Systems Limited
 *
 * \section LICENSE
 *
 * This file is part of the srsUE library.
 *
 * srsUE is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * srsUE is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * A copy of the GNU Affero General Public License can be found in
 * the LICENSE file in the top-level directory of this distribution
 * and at http://www.gnu.org/licenses/.
 *
 */


/******************************************************************************
 *  File:         timers.h
 *  Description:  Manually incremented timers. Call a callback function upon
 *                expiry.
 *  Reference:
 *****************************************************************************/

#ifndef TIMERS_H
#define TIMERS_H

#include <stdio.h>
#include <stdint.h>
#include <vector>
#include <time.h>

namespace srslte {
  
class timer_callback 
{
  public: 
    virtual void timer_expired(uint32_t timer_id) = 0; 
}; 
  
class timers
{
public:
  class timer
  {
  public:
    timer(uint32_t id_=0) {id = id_; counter = 0; timeout = 0; running = false; callback = NULL; }
    void set(timer_callback *callback_, uint32_t timeout_) {
      callback = callback_; 
      timeout = timeout_; 
      reset();
    }
    bool is_running() {
      return (counter < timeout) && running; 
    }
    bool is_expired() {
      return counter == timeout || !running; 
    }
    uint32_t get_timeout() {
      return timeout; 
    }
    void reset() {
      counter = 0; 
    }
    void step() {
      if (running) {
        counter++; 
        if (is_expired()) {
          running = false; 
          callback->timer_expired(id); 
        }        
      }
    }
    void stop() {
      running = false; 
    }
    void run() {
      running = true; 
    }
    uint32_t id; 
  private: 
    timer_callback *callback; 
    uint32_t timeout; 
    uint32_t counter; 
    bool running; 
  };
  
  timers(uint32_t nof_timers_) : timer_list(nof_timers_) {
    nof_timers = nof_timers_; 
    next_timer = 0;
    for (uint32_t i=0;i<nof_timers;i++) {
      timer_list[i].id = i; 
    }
  }
  
  void step_all() {
    for (int i=0;i<nof_timers;i++) {
      get(i)->step();
    }
  }
  void stop_all() {
    for (int i=0;i<nof_timers;i++) {
      get(i)->stop();
    }
  }
  void run_all() {
    for (int i=0;i<nof_timers;i++) {
      get(i)->run();
    }
  }
  void reset_all() {
    for (int i=0;i<nof_timers;i++) {
      get(i)->reset();
    }
  }
  timer *get(uint32_t i) {
    if (i < nof_timers) {
      return &timer_list[i];       
    } else {
      printf("Error accessing invalid timer %d (Only %d timers available)\n", i, nof_timers);
      return NULL; 
    }
  }
  uint32_t get_unique_id() {
    if (next_timer == nof_timers){
      printf("No more unique timer ids (Only %d timers available)\n", nof_timers);
      next_timer = 0;
    }
    return next_timer++;
  }
private:
  uint32_t nof_timers; 
  uint32_t next_timer;
  std::vector<timer>   timer_list;   
};

} // namespace srslte
  
#endif // TIMERS_H
