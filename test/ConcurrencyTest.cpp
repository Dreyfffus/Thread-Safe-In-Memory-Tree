#include "pch.h"
#include "CppUnitTest.h"

#include <thread>
#include <vector>
#include <string>
#include <atomic>
#include <chrono>

#include "../concurrent_tree/concurrent.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace ConcurrencyTest
{
	TEST_CLASS(ConcurrencyTest)
	{
	public:
		
		TEST_METHOD(EmptyTree_GetReturnNull)
		{
			sync::ConcurrentTree tree;
			auto v = tree.get(data("missing"));
			Assert::IsTrue(v == tree.NULL_VALUE, L"Missing key must return NULL_VALUE");
		}

		TEST_METHOD(SinglePutGet)
		{
			sync::ConcurrentTree tree;
			tree.put(data("a"), data("v1"));
			auto v = tree.get(data("a"));
			Assert::IsTrue(v == data("v1"));
			Assert::IsTrue(tree.get(data("z")) == tree.NULL_VALUE);
		}

		TEST_METHOD(PutGetUpdate)
		{
			sync::ConcurrentTree tree;
			tree.put(data("k"), data("v1"));
			tree.put(data("k"), data("v2"));
			Assert::IsTrue(tree.get(data("k")) == data("v2"));
		}

		TEST_METHOD(AscendingInsertsArePresent)
		{
			sync::ConcurrentTree tree;
			for (int i = 0; i < 100; i++) {
				auto s = std::to_string(i);
				tree.put(data(s), data(s));
			}
			for (int i = 0; i < 100; i++) {
				auto s = std::to_string(i);
				Assert::IsTrue(tree.get(data(s)) == data(s));
			}
		}

		TEST_METHOD(DescendingInsertsArePresent)
		{
			sync::ConcurrentTree tree;
			for (int i = 100; i > 0; --i) {
				auto s = std::to_string(i);
				tree.put(data(s), data(s));
			}
			for (int i = 100; i > 0; --i) {
				auto s = std::to_string(i);
				Assert::IsTrue(tree.get(data(s)) == data(s));
			}
		}

		TEST_METHOD(ConcurrentWritersArePresent)
		{
			sync::ConcurrentTree tree;

			const int threads = 4;
			const int perThread = 200;

			auto worker = [&](int id) {
				for (int i = 0; i < perThread; i++) {
					std::string k = "t" + std::to_string(id) + "_" + std::to_string(i);
					tree.put(data(k), data(k));
					if ((i & 31) == 0) std::this_thread::yield(); // more concurrence
				}
			};

			std::vector<std::thread> t;
			t.reserve(threads);
			for (int i = 0; i < threads; ++i) t.emplace_back(worker, i);
			for (auto& thread : t) thread.join();

			for (int id = 0; id < threads; ++id) {
				for (int i = 0; i < perThread; ++i) {
					std::string k = "t" + std::to_string(id) + "_" + std::to_string(i);
					Assert::IsTrue(tree.get(data(k)) == data(k));
				}
			}
		}

		TEST_METHOD(ConcurrentReadersDuringWrites_NoCrashesAndEventuallyPresent)
		{
			sync::ConcurrentTree tree;

			std::atomic<bool> run{ true };
			std::thread writer([&] {
				for (int i = 0; i < 500; ++i) {
					auto s = std::to_string(i);
					tree.put(data(s), data(s));
					if ((i & 15) == 0) std::this_thread::sleep_for(std::chrono::milliseconds(1));
				}
				run = false;
				});

			// readers spin while writer inserts
			std::thread reader1([&] {
				int i = 0;
				while (run) {
					auto key = data(std::to_string(i++ % 500));
					(void)tree.get(key);
				}
				});
			std::thread reader2([&] {
				int i = 0;
				while (run) {
					auto key = data(std::to_string((499 - (i++ % 500))));
					(void)tree.get(key);
				}
				});

			writer.join();
			reader1.join();
			reader2.join();

			// After writer finished, all keys must be present
			for (int i = 0; i < 500; ++i) {
				auto s = std::to_string(i);
				Assert::IsTrue(tree.get(data(s)) == data(s));
			}
		}

	};
}
