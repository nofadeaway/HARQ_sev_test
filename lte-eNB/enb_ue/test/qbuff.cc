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


#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "srslte/utils/vector.h"
#include "common/qbuff.h"

namespace srslte {
  

qbuff::qbuff()
{
  nof_messages=0; 
  max_msg_size=0;
  wp = 0; 
  rp = 0; 
  buffer = NULL;
  packets = NULL; 
}

qbuff::~qbuff()
{
  free(buffer);
  free(packets);
}

bool qbuff::init(uint32_t nof_messages_, uint32_t max_msg_size_)  //nof_messages为64--nof_PDU,一个HARQ进程最多缓存多少个PDU；max_msg_size为单个PDU的最大长度
{
  nof_messages = nof_messages_; 
  max_msg_size = max_msg_size_; 
  
  buffer  = (uint8_t*) srslte_vec_malloc(nof_messages*max_msg_size);   //从申请的空间来看，buffer是真正的存储空间，packets是buff的标识
  packets = (pkt_t*)   srslte_vec_malloc(nof_messages*sizeof(pkt_t));     //pkt_t为一个定义在qbuff中的结构体，成员为是否有效，长度，指针
  if (buffer && packets) {
    bzero(buffer, nof_messages*max_msg_size);   //bzero 将字节字符串前n个字节置为0
    bzero(packets, nof_messages*sizeof(pkt_t));
    flush();
    return true; 
  } else {
    return false; 
  }
}

void qbuff::flush()        //在init函数里面被调用了
{
  wp = 0; 
  rp = 0; 
  for (int i=0;i<nof_messages;i++) {
    packets[i].valid = false; 
    packets[i].ptr   = &buffer[i*max_msg_size];   //pkt中指针指向申请的缓冲区 
    packets[i].len   = 0; 
  }  
}

bool qbuff::isempty()
{
  return !packets[rp].valid;
}

bool qbuff::isfull()
{
  return packets[wp].valid; 
}


void* qbuff::request()
{
  if (!isfull()) {
    return packets[wp].ptr; 
  } else {
    return NULL; 
  }
}

bool qbuff::push(uint32_t len)
{
  packets[wp].len = len; 
  packets[wp].valid = true; 
  wp += (wp+1 >= nof_messages)?(1-nof_messages):1;  //循环了
  return true; 
}

void* qbuff::pop()
{
  return pop(NULL);
}

void* qbuff::pop(uint32_t* len)          //pop操作会改变len的值
{
  if (!isempty()) {
    if (len) {
      *len = packets[rp].len;
    }
    return packets[rp].ptr; 
  } else {
    return NULL; 
  }
}

void* qbuff::pop(uint32_t* len, uint32_t idx)
{
  if (idx == 0) {
    return pop(len);
  } else {
    uint32_t rpp = rp; 
    uint32_t i   = 0; 
    while(i<idx && packets[rpp].valid) {          //最终使得 rpp=rp+idx
      rpp += (rpp+1 >= nof_messages)?(1-nof_messages):1; 
      i++;
    }
    if (packets[rpp].valid) {
      if (len) {
        *len = packets[rpp].len;
      }
      return packets[rpp].ptr; 
    } else {
      return NULL; 
    }    
  }
}

void qbuff::release()
{
  packets[rp].valid = false; 
  packets[rp].len = 0; 
  rp += (rp+1 >= nof_messages)?(1-nof_messages):1; 
}

bool qbuff::send(void* buffer, uint32_t msg_size)   //这个是真正的将数据写入qbuff存储的函数
{
  if (msg_size <= max_msg_size) {
    void *ptr = request();         //request()是return packets[wp].ptr;
    if (ptr) {
      memcpy(ptr, buffer, msg_size); //void *memcpy(void *dest, const void *src, size_t n);从源src所指的内存地址的起始位置开始拷贝n个字节到目标dest所指的内存地址的起始位置中
      return push(msg_size);
    } else {
      printf("No ptr\n");
      return false; 
    }
  } else {
    return false; 
  }
}

uint32_t qbuff::pending_data()
{
  uint32_t total_len = 0; 
  for (int i=0;i<nof_messages;i++) {
    total_len += packets[i].len;
  }
  return total_len; 
}

uint32_t qbuff::pending_msgs()
{
  uint32_t nof_msg = 0; 
  for (int i=0;i<nof_messages;i++) {
    nof_msg += packets[i].valid?1:0;
  }
  return nof_msg;
}

uint32_t qbuff::max_msgs()
{
  return nof_messages;
}

// Move packets between queues with only 1 memcpy
void qbuff::move_to(qbuff *dst) {
  uint32_t len; 
  void *ptr_src = pop(&len);
  if (ptr_src) {
    void *ptr_dst = dst->request();
    if (ptr_dst) {
      memcpy(ptr_dst, ptr_src, len);
      dst->push(len);
      release();
    }
  }
}
    


int qbuff::recv(void* buffer, uint32_t buffer_size)
{
  uint32_t len; 
  void *ptr = pop(&len);
  if (ptr) {
    if (len <= buffer_size) {
      memcpy(buffer, ptr, len);   
      release();
      return len; 
    } else {
      return -1; 
    }
  } else {
    return 0; 
  }
}

//FX
uint32_t qbuff::rp_is()    //声明时不要掉了类名 qbuff::
{
  //uint32_t rp_temp=rp;
  return rp;
}

uint32_t qbuff::wp_is()
{
  //uint32_t wp_temp=wp;
  return wp;
}
//FX

}