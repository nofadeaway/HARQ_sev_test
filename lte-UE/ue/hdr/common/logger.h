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
 * File:        logger.h
 * Description: Common log object. Maintains a queue of log messages
 *              and runs a thread to read messages and write to file.
 *              Multiple producers, single consumer. If full, producers
 *              increase queue size. If empty, consumer blocks.
 *****************************************************************************/

#ifndef LOGGER_H
#define LOGGER_H

#include <stdio.h>
#include <string>
#include <boost/shared_ptr.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/condition.hpp>
#include <boost/circular_buffer.hpp>

namespace srslte {

typedef boost::shared_ptr<std::string> str_ptr;

class logger
{
public:
  logger();
  logger(std::string file);
  ~logger();
  void init(std::string file);
  void log(const char *msg);
  void log(str_ptr msg);

private:
  static void* start(void *input);
  void reader_loop();
  void flush();

  FILE*                               logfile;
  bool                                inited;
  bool                                not_done;
  std::string                         filename;
  boost::condition                    not_empty;
  boost::condition                    not_full;
  boost::mutex                        mutex;
  pthread_t                           thread;
  boost::circular_buffer<str_ptr>     buffer;
};

} // namespace srsue

#endif // LOGGER_H
