// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "task_runner.h"

#include <atomic>
#include <utility>
#include <iostream>

namespace flutter {

TaskRunner::TaskRunner(std::thread::id main_thread_id,
                       CurrentTimeProc get_current_time,
                       const TaskExpiredCallback& on_task_expired)
    : main_thread_id_(main_thread_id),
      get_current_time_(get_current_time),
      on_task_expired_(std::move(on_task_expired)) {}

bool TaskRunner::RunsTasksOnCurrentThread() const {
  return std::this_thread::get_id() == main_thread_id_;
}

void TaskRunner::PostFlutterTask(FlutterTask flutter_task,
                                      uint64_t flutter_target_time_nanos) {
  Task task;
  task.fire_time = TimePointFromFlutterTime(flutter_target_time_nanos);
  task.variant = flutter_task;
  EnqueueTask(std::move(task));
}

void TaskRunner::PostTask(TaskClosure closure) {
  Task task;
  task.fire_time = TaskTimePoint::clock::now();
  task.variant = std::move(closure);
  EnqueueTask(std::move(task));
}

void TaskRunner::EnqueueTask(Task task) {
  static std::atomic_uint64_t sGlobalTaskOrder(0);

  task.order = ++sGlobalTaskOrder;

  std::lock_guard<std::mutex> lock(task_queue_mutex_);
  task_queue_.push(task);
}

std::chrono::nanoseconds TaskRunner::ProcessTasks() {
  const TaskTimePoint now = TaskTimePoint::clock::now();

  std::vector<Task> expired_tasks;

  // Process expired tasks.
  {
    std::lock_guard<std::mutex> lock(task_queue_mutex_);
    while (!task_queue_.empty()) {
      const auto& top = task_queue_.top();
      // If this task (and all tasks after this) has not yet expired, there is
      // nothing more to do. Quit iterating.
      if (top.fire_time > now) {
        break;
      }

      // Make a record of the expired task. Do NOT service the task here
      // because we are still holding onto the task queue mutex. We don't want
      // other threads to block on posting tasks onto this thread till we are
      // done processing expired tasks.
      expired_tasks.push_back(task_queue_.top());

      // Remove the tasks from the delayed tasks queue.
      task_queue_.pop();
    }
  }

  // Fire expired tasks.
  {
    // Flushing tasks here without holing onto the task queue mutex.
    for (const auto& task : expired_tasks) {
      if (auto flutter_task = std::get_if<FlutterTask>(&task.variant)) {
        on_task_expired_(flutter_task);
      } else if (auto closure = std::get_if<TaskClosure>(&task.variant))
        (*closure)();
    }
  }

  // Calculate duration to sleep for on next iteration.
  {
    std::lock_guard<std::mutex> lock(task_queue_mutex_);
    if (task_queue_.empty()) {
      return TaskTimePoint::max().time_since_epoch();
    }
    const auto next_wake = task_queue_.top().fire_time;
    return std::min(next_wake - now, std::chrono::nanoseconds::max());
  }
}

TaskRunner::TaskTimePoint TaskRunner::TimePointFromFlutterTime(
    uint64_t flutter_target_time_nanos) const {
  const auto now = TaskTimePoint::clock::now();
  const auto flutter_duration = flutter_target_time_nanos - get_current_time_();
  return now + std::chrono::nanoseconds(flutter_duration);
}

}  // namespace flutter
