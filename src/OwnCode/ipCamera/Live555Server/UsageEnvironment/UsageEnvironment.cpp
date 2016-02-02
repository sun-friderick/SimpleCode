/**********
This library is free software; you can redistribute it and/or modify it under
the terms of the GNU Lesser General Public License as published by the
Free Software Foundation; either version 2.1 of the License, or (at your
option) any later version. (See <http://www.gnu.org/copyleft/lesser.html>.)

This library is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
more details.

You should have received a copy of the GNU Lesser General Public License
along with this library; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
**********/
// Copyright (c) 1996-2015 Live Networks, Inc.  All rights reserved.
// Usage Environment
// Implementation

#include "UsageEnvironment.hh"

Boolean UsageEnvironment::reclaim() {
  // We delete ourselves only if we have no remainining state:
  if (liveMediaPriv == NULL && groupsockPriv == NULL) {
    delete this;
    return True;
  }

  return False;
}

UsageEnvironment::UsageEnvironment(TaskScheduler& scheduler)
  : liveMediaPriv(NULL), groupsockPriv(NULL), fScheduler(scheduler) {
}

UsageEnvironment::~UsageEnvironment() {
}

// By default, we handle 'should not occur'-type library errors by calling abort().  Subclasses can redefine this, if desired.
// (If your runtime library doesn't define the "abort()" function, then define your own (e.g., that does nothing).)
void UsageEnvironment::internalError() {
  abort();
}


//////////////////////// Class TaskScheduler implementation /////////////////////////
/**
 *  
 *  三种任务命名为：socket handler,  event handler,  delay task。
 *  
 *  这三种任务的特点是，前两个加入执行队列后会一直存在，而delay task在执行完一次后会立即弃掉;
 *      socket handler 保存在队列 BasicTaskScheduler0::HandlerSet* fHandlers 中;
 *      event handler  保存在数组 BasicTaskScheduler0::TaskFunc * fTriggeredEventHandlers[MAX_NUM_EVENT_TRIGGERS]中;
 *      delay task 保存在队列 BasicTaskScheduler0::DelayQueue fDelayQueue 中。
 *  
 *  三种任务的执行函数的定义：
 *      socket handler: typedef void BackgroundHandlerProc(void* clientData, int mask); 
 *      event handler : typedef void TaskFunc(void* clientData);
 *      delay task: typedef void TaskFunc(void* clientData);//跟event handler一样。
 *  
 *  向任务调度对象添加三种任务的函数的样子：
 *      socket handler: void setBackgroundHandling(int socketNum, int conditionSet　,BackgroundHandlerProc* handlerProc, void* clientData)
 *      event handler : EventTriggerId createEventTrigger(TaskFunc* eventHandlerProc)
 *      delay task: TaskToken scheduleDelayedTask(int64_t microseconds, TaskFunc* proc,void* clientData)
 *  
 *  socket handler添加时为什么需要那些参数呢？
 *      socketNum是需要的，因为要select socket（socketNum即是socket()返回的那个socket对象）。
 *      conditionSet也是需要的，它用于表明socket在select时查看哪种装态，是可读？可写？还是出错？
 *      proc和clientData这两个参数就不必说了（真有不明白的吗？）。
 *      再看 BackgroundHandlerProc 的参数，socketNum不必解释，mask是什么呢？它正是对应着conditionSet，但它表明的是select之后的结果，比如一个socket可能需要检查其读/写状态，而当前只能读，不能写，那么mask中就只有表明读的位被设置。
 *  
 *  event handler是被存在数组中。
 *      数组大小固定，是32项，用EventTriggerId来表示数组中的项，EventTriggerId是一个32位整数，因为数组是32项，所以用EventTriggerId中的第n位置１表明对应数组中的第n项。
 *      成员变量fTriggersAwaitingHandling也是EventTriggerId类型，它里面置1的那些位对应了数组中所有需要处理的项。
 *      这样做节省了内存和计算，但降低了可读性，呵呵，而且也不够灵活，只能支持32项或64项，其它数量不被支持。
 *  
 **/
TaskScheduler::TaskScheduler() {
}

TaskScheduler::~TaskScheduler() {
}

void TaskScheduler::rescheduleDelayedTask(TaskToken& task,
					  int64_t microseconds, TaskFunc* proc,
					  void* clientData) {
  unscheduleDelayedTask(task);
  task = scheduleDelayedTask(microseconds, proc, clientData);
}

// By default, we handle 'should not occur'-type library errors by calling abort().  Subclasses can redefine this, if desired.
void TaskScheduler::internalError() {
  abort();
}
