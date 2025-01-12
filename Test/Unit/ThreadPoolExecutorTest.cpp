/*
 * Copyright 2019 LogMeIn
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "gmock/gmock.h"

#include "boost/chrono.hpp"
#include "boost/thread.hpp"
#include "boost/thread/future.hpp"

#include <future>
#include <thread>

#include "asyncly/executor/InlineExecutor.h"
#include "asyncly/executor/ThreadPoolExecutorController.h"

namespace asyncly {

using namespace testing;

class ThreadPoolExecutorTest : public Test {
  public:
    void SetUp() override
    {
    }
};

TEST_F(ThreadPoolExecutorTest, shouldInitializeWithOneThread)
{
    ThreadPoolExecutorController::create(1);
}

TEST_F(ThreadPoolExecutorTest, shouldInializeWithMultipleThreads)
{
    ThreadPoolExecutorController::create(10);
}

TEST_F(ThreadPoolExecutorTest, DISABLED_shouldRunClosuresOnMultipleThreads)
{
    const auto numThreads = 5;

    std::array<boost::promise<std::thread::id>, numThreads> threadIdPromises;
    std::array<boost::shared_future<std::thread::id>, numThreads> threadIdFutures;
    std::transform(
        threadIdPromises.begin(),
        threadIdPromises.end(),
        threadIdFutures.begin(),
        [](boost::promise<std::thread::id>& p) { return p.get_future(); });

    auto closure = [&threadIdPromises, &threadIdFutures](int i) {
        auto threadId = std::this_thread::get_id();
        threadIdPromises[i].set_value(threadId);
        boost::wait_for_all(threadIdFutures.begin(), threadIdFutures.end());
    };

    {
        auto executorController = ThreadPoolExecutorController::create(numThreads);
        auto executor = executorController->get_executor();
        for (auto i = 0; i < numThreads; i++) {
            executor->post(std::bind(closure, i));
        }
    }

    boost::wait_for_all(threadIdFutures.begin(), threadIdFutures.end());
    std::array<std::thread::id, numThreads> threadIds;
    std::transform(
        threadIdFutures.begin(),
        threadIdFutures.end(),
        threadIds.begin(),
        [](boost::shared_future<std::thread::id>& f) { return f.get(); });

    std::sort(threadIds.begin(), threadIds.end());
    EXPECT_EQ(threadIds.end(), std::adjacent_find(threadIds.begin(), threadIds.end()));
}

TEST_F(ThreadPoolExecutorTest, shouldSupportDestructionInWorkerThread)
{
    for (int i = 0; i < 1000; i++) {
        std::promise<std::thread::id> p;
        {
            auto executorController = ThreadPoolExecutorController::create(1);
            auto executor = executorController->get_executor();
            executor->post([executor, &p]() {
                executor->post([executor, &p]() {
                    executor->post([executor, &p]() { p.set_value(std::this_thread::get_id()); });
                });
            });
        }
        EXPECT_NE(p.get_future().get(), std::this_thread::get_id());
    }
}

TEST_F(ThreadPoolExecutorTest, shouldFinishAllTasksBeforeDestruction)
{
    auto executorController = ThreadPoolExecutorController::create(1);
    auto executor = executorController->get_executor();
    std::promise<void> sync;

    std::function<void(int)> recurse = [&recurse, &sync](int i) {
        if (i < 1000) {
            this_thread::get_current_executor()->post(std::bind(recurse, i + 1));
        } else {
            sync.set_value();
        }
    };

    executor->post([recurse]() { recurse(0); });
    executor.reset();

    EXPECT_NO_THROW(sync.get_future().get());
}

TEST_F(ThreadPoolExecutorTest, shouldFinishAllThreadsBeforeExecutorDestruction)
{
    auto executorController = ThreadPoolExecutorController::create(1);
    auto executor = executorController->get_executor();

    std::promise<void> sync;
    std::future<void> fsync = sync.get_future();

    std::promise<void> sync_done;
    std::future<void> fsync_done = sync_done.get_future();

    auto done = std::make_shared<bool>(false);
    auto payload = std::shared_ptr<bool>(new bool(true), [&done, &sync_done](bool* b) mutable {
        delete b;

        // make it somewhat more likely to fail
        boost::this_thread::sleep_for(boost::chrono::milliseconds(100));
        *done = true;
        sync_done.set_value();
    });

    executor->post([&fsync, payload]() { fsync.get(); });

    payload.reset();

    executor.reset();
    // makes sure external executor pointer has been released, before task is executed (and possibly
    // destroyed)
    sync.set_value();

    executorController->finish();
    EXPECT_TRUE(*done);

    // don't crash in case of error
    fsync_done.get();
}

TEST_F(ThreadPoolExecutorTest, shouldSupportNonCopyableTask)
{
    auto executorController = ThreadPoolExecutorController::create(1);
    auto executor = executorController->get_executor();
    std::promise<void> sync;
    auto future = sync.get_future();

    executor->post([promise{ std::move(sync) }]() mutable { promise.set_value(); });
    executor.reset();

    EXPECT_NO_THROW(future.get());
}

TEST_F(ThreadPoolExecutorTest, shouldNotThrowOnGetCurrentExecutorInNestedTasks)
{
    // this case can just happen with the "inline executor" which is currently used in multiple
    // tests, I would question the "inline executor" in general, is it still an executor?
    auto executor = ::asyncly::InlineExecutor::create();
    std::promise<void> promiseFinished;
    executor->post([&executor, &promiseFinished]() {
        executor->post([]() {});
        EXPECT_NO_THROW(::asyncly::this_thread::get_current_executor());
        promiseFinished.set_value();
    });
    promiseFinished.get_future().wait();
}
} // namespace asyncly
