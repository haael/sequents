

#ifndef LOGICAL_SYNC_HH
#define LOGICAL_SYNC_HH

#include <chrono>
#include <exception>
#include <initializer_list>
#include <mutex>
#include <utility>
#include <functional>

#include "errors.hh"
#include "logical.hh"

namespace Logical
{

using std::adopt_lock_t;
using std::atomic_bool;
using std::current_exception;
using std::defer_lock_t;
using std::exception_ptr;
using std::forward;
using std::initializer_list;
using std::move;
using std::pair;
using std::rethrow_exception;
using std::thread;
using std::try_to_lock_t;
using std::unique_lock;
using std::reference_wrapper;
using std::mutex;
using std::condition_variable;


template <typename A, typename B>
using chrono_duration = std::chrono::duration<A, B>;

template <typename A, typename B>
using chrono_time_point = std::chrono::time_point<A, B>;

template<typename key, typename value>
using unordered_map_sane = std::unordered_map<key, value>;

template<typename key>
using unordered_set_sane = std::unordered_set<key>;


class Thread : public thread
{
private:
	static mutex finished_access;
	static condition_variable finished;

	struct Extension
	{
		exception_ptr error;
		atomic_bool running;
		
		Extension(bool r)
		: error(nullptr), running(r)
		{
		}

	};
	
	Extension* extension;

public:
	exception_ptr error(void) const
	{
		if(!extension)
			return nullptr;
		return extension->error;
	}
	
	bool running(void) const
	{
		if(!extension)
			return false;
		return extension->running;
	}

private:
	template <typename Fn, typename... Args>
	static void task(Extension* extension, Fn&& fn, Args&&... args)
	{
		logical_assert(extension, "Extension pointer invalid");
		
		try
		{
			fn(args...);
		}
		catch(...)
		{
			unique_lock<mutex> lock(finished_access);
			extension->error = current_exception();
		}
		
		{
			unique_lock<mutex> lock(finished_access);
			extension->running = false;
			finished.notify_all();
		}
	}

public:
	Thread(void) noexcept
	{
	}
	
	template <typename Fn, typename... Args>
	explicit Thread(Fn&& fn, Args&&... args)
	{
		extension = new Extension(true);
		thread::operator=(thread(task<Fn, Args...>, extension, fn, args...));
	}
	
	Thread(const Thread&) = delete;
	
	Thread(Thread&& other) noexcept
	 : extension(other.extension)
	 , thread(static_cast<thread&&>(other))
	{
		other.extension = nullptr;
	}
	
	Thread& operator=(Thread&& rhs) noexcept
	{
		thread::operator=(static_cast<thread&&>(rhs));
		extension = rhs.extension;
		rhs.extension = nullptr;
		return *this;
	}
	
	Thread& operator=(const Thread&) = delete;
	
	void join(void)
	{
		thread::join();
		if(error())
			rethrow_exception(error());
	}
	
	void raw_join(void)
	{
		thread::join();
	}
	
	template <typename Collection>
	static void finalize(Collection&& all_threads)
	{
		bool running = true;
		exception_ptr error = nullptr;
		
		while(running)
		{
			unique_lock<mutex> lock(finished_access);
			
			running = false;

			for(Thread& thr : all_threads)
			{
				if(thr.running())
					running = true;
				if(thr.error())
				{
					error = thr.error();
					break;
				}
			}
						
			if(running)
				finished.wait(lock);
		}

		for(Thread& thr : all_threads)
		{
			logical_assert(!thr.running(), "Thread should not be running.");

			if(thr.error())
			{
				error = thr.error();
				break;
			}
		}
		
		if(!error)
		{
			for(Thread& thr : all_threads)
				if(thr.joinable())
					thr.raw_join();
		}
		else
		{
			for(Thread& thr : all_threads)
				if(thr.joinable())
					thr.detach();
			rethrow_exception(error);
		}
	}
	
	static void finalize(initializer_list<reference_wrapper<Thread>>&& all_threads)
	{
		finalize<initializer_list<reference_wrapper<Thread>>>(move(all_threads));
	}
	
	~Thread(void)
	{
		if(extension)
			delete extension;
	}
};


#ifndef LOGICAL_SYNC_HH_thread_static_vars
#define LOGICAL_SYNC_HH_thread_static_vars
mutex Thread::finished_access;
condition_variable Thread::finished;
#endif


template <typename SharedMutex>
class ReadLockable
{
private:
	SharedMutex& access;

public:
	ReadLockable(SharedMutex& a)
	 : access(a)
	{
	}

	ReadLockable(const ReadLockable& cp)
	 : access(cp.access)
	{
	}

	ReadLockable(ReadLockable&& mv)
	 : access(mv.access)
	{
	}

	ReadLockable& operator=(const ReadLockable&) = delete;
	
	SharedMutex& write_lockable(void) { return access; }

	void lock(void) { access.lock_shared(); }

	bool try_lock(void) { return access.try_lock_shared(); }

	template <typename Duration>
	bool try_lock_for(Duration&& timeout_duration)
	{
		return access.try_lock_shared_for(forward<Duration>(timeout_duration));
	}

	template <typename Time>
	bool try_lock_until(Time&& timeout_time)
	{
		return access.try_lock_shared_until(forward<Time>(timeout_time));
	}

	void unlock(void) { access.unlock_shared(); }
};

template <typename SharedMutex>
class SharedLock : public unique_lock<ReadLockable<SharedMutex>>
{
private:
	unique_lock<SharedMutex>* write_lock;

public:
	typedef typename unique_lock<ReadLockable<SharedMutex>>::mutex_type mutex_type;

	SharedLock() noexcept
	 : unique_lock<ReadLockable<SharedMutex>>()
	 , write_lock(nullptr)
	{
	}

	SharedLock(SharedLock&& other) noexcept
	 : unique_lock<ReadLockable<SharedMutex>>(other)
	 , write_lock(other.write_lock)
	{
		other.write_lock = nullptr;
	}

	explicit SharedLock(ReadLockable<SharedMutex>& m)
	 : unique_lock<ReadLockable<SharedMutex>>(m)
	 , write_lock(nullptr)
	{
	}

	SharedLock(mutex_type& m, defer_lock_t t) noexcept
	 : unique_lock<ReadLockable<SharedMutex>>(m, t)
	 , write_lock(nullptr)
	{
	}

	SharedLock(mutex_type& m, try_to_lock_t t)
	 : unique_lock<ReadLockable<SharedMutex>>(m, t)
	 , write_lock(nullptr)
	{
	}
	SharedLock(mutex_type& m, adopt_lock_t t)
	 : unique_lock<ReadLockable<SharedMutex>>(m, t)
	 , write_lock(nullptr)
	{
	}

	template <class Rep, class Period>
	SharedLock(mutex_type& m, const chrono_duration<Rep, Period>& timeout_duration)
	 : unique_lock<ReadLockable<SharedMutex>>(m, timeout_duration)
	 , write_lock(nullptr)
	{
	}

	template <class Clock, class Duration>
	SharedLock(mutex_type& m, const chrono_time_point<Clock, Duration>& timeout_time)
	 : unique_lock<ReadLockable<SharedMutex>>(m, timeout_time)
	 , write_lock(nullptr)
	{
	}

	bool is_upgraded(void) const { return (bool)write_lock; }

	unique_lock<SharedMutex>& write(void)
	{
		if(is_upgraded())
			return *write_lock;
		else
			throw LockingError("Write lock not active.");
	}

	template <typename... Arguments>
	unique_lock<SharedMutex>& upgrade(Arguments... arguments)
	{
		if(!is_upgraded())
		{
			write_lock = new unique_lock<SharedMutex>(this->mutex()->write_lockable(), arguments...);
			return *write_lock;
		}
		else
			throw DeadlockError("Write lock already active.");
	}

	void downgrade(void)
	{
		if(is_upgraded())
			delete write_lock;
		else
			throw LockingError("Write lock not active.");
	}

	~SharedLock(void)
	{
		if(write_lock)
			delete write_lock;
		//unique_lock<ReadLockable<SharedMutex>>::~unique_lock(); FIXME
	}
};


template <typename Map, typename SharedMutex, template <typename KeyType, typename MappedType> typename InternalMap = unordered_map_sane, template <typename KeyType> typename InternalSet = unordered_set_sane>
class Transaction
{
public:
	typedef typename Map::key_type key_type;
	typedef typename Map::mapped_type mapped_type;

private:
	Map& back_map;
	ReadLockable<SharedMutex> access_mutex;
	InternalMap<key_type, mapped_type> reads, writes;
	InternalMap<key_type, bool> counts;
	InternalSet<key_type> erases;

	class Accessor
	{
	private:
		Transaction& trans;
		key_type key;

	public:
		Accessor(Transaction& t, key_type k)
		 : trans(t)
		 , key(k)
		{
		}

		bool operator == (mapped_type value)
		{
			return (mapped_type)(*this) == value;
		}

		bool operator == (Accessor& accessor)
		{
			return (mapped_type)(*this) == (mapped_type)accessor;
		}

		mapped_type operator=(mapped_type value)
		{
			if(trans.erases.count(key))
				trans.erases.erase(key);
			return trans.writes[key] = value;
		}

		mapped_type operator=(Accessor& accessor) { return (*this) = (mapped_type)accessor; }

		operator mapped_type(void)
		{
			if(trans.writes.count(key))
			{
				return trans.writes[key];
			}
			else if(trans.erases.count(key))
			{
				trans.erases.erase(key);
				return trans.writes[key];
			}
			else if(trans.reads.count(key))
			{
				return trans.reads[key];
			}
			else
			{
				SharedLock<SharedMutex> lock(trans.access_mutex);

				if(trans.back_map.count(key))
				{
					const auto value = trans.back_map[key];
					lock.unlock();
					return trans.reads[key] = value;
				}
				else
				{
					lock.unlock();
					return trans.writes[key];
				}
			}
		}
	};

	enum class Mode : uint8_t
	{
		INVALID,
		READS,
		WRITES,
		NATIVE,
		END
	};

	class Iterator
	{
	protected:
		Transaction& trans;
		Mode mode;
		union {
			typename Map::const_iterator native_iterator;
			typename InternalMap<key_type, mapped_type>::const_iterator writes_iterator;
			typename InternalMap<key_type, mapped_type>::const_iterator reads_iterator;
		};

		key_type current_key(void) const
		{
			switch(mode)
			{
			case Mode::WRITES:
				return writes_iterator->first;

			case Mode::READS:
				return reads_iterator->first;

			case Mode::NATIVE:
			{
				SharedLock<SharedMutex> lock(trans.access_mutex);
				return native_iterator->first;
			}

			default:
				throw IteratorError("Iterator in wrong state.", 0, &trans, nullptr);
			}
		}

	private:
		void set_mode(Mode m)
		{
			mode = m;
			switch(mode)
			{
			case Mode::WRITES:
				writes_iterator = trans.writes.cbegin();
				break;

			case Mode::READS:
				reads_iterator = trans.reads.cbegin();
				break;

			case Mode::NATIVE:
			{
				SharedLock<SharedMutex> lock(trans.access_mutex);
				native_iterator = trans.back_map.cbegin();
				break;
			}

			default:
				break;
			}
		}

		void advance(void)
		{
			switch(mode)
			{
			case Mode::WRITES:
				if(++writes_iterator >= trans.writes.cend())
					set_mode(Mode::READS);
				break;

			case Mode::READS:
				if(++reads_iterator >= trans.reads.cend())
					set_mode(Mode::NATIVE);
				break;

			case Mode::NATIVE:
			{
				SharedLock<SharedMutex> lock(trans.access_mutex);
				if(++native_iterator >= trans.back_map.cend())
					set_mode(Mode::END);
				break;
			}

			default:
				throw IteratorError("Iterator in wrong state.", 0, &trans, nullptr);
			}
		}

		bool valid(void)
		{
			const auto key = current_key();

			switch(mode)
			{
			case Mode::WRITES:
				return !trans.erases.count(key);

			case Mode::READS:
				return !trans.erases.count(key) && !trans.writes.count(key);

			case Mode::NATIVE:
			{
				bool key_in_native_map = false;
				{
					SharedLock<SharedMutex> lock(trans.access_mutex);
					key_in_native_map = trans.back_map.count(key);
				}
				return !trans.erases.count(key) && !trans.writes.count(key) && !key_in_native_map;
			}

			default:
				throw IteratorError("Iterator in wrong state.", 0, &trans, nullptr);
			}
		}

		bool completed(void) { return mode == Mode::END; }

	public:
		Iterator(Transaction& t, Mode m)
		 : trans(t)
		{
			init(m);
		}

		Iterator(Transaction& t, key_type k)
		 : trans(t)
		{
			init(Mode::WRITES);
			while(!completed() && !valid() && current_key() != k)
				advance();
		}

		Iterator& operator++(void)
		{
			do
				advance();
			while(!completed() && !valid());
			return *this;
		}

		bool operator==(const Iterator& other) const
		{
			return (&trans == &other.trans) && (mode == other.mode) && ((mode == Mode::NATIVE) ? (native_iterator == other.native_iterator) : true)
			    && ((mode == Mode::WRITES) ? (writes_iterator == other.writes_iterator) : true)
			    && ((mode == Mode::READS) ? (reads_iterator == other.reads_iterator) : true);
		}

		bool operator!=(const Iterator& other) const { return !((*this) == other); }

		bool operator<=(const Iterator& other) const
		{
			return (&trans == &other.trans)
			    && ((mode == Mode::WRITES && (other.mode == Mode::READS || other.mode == Mode::NATIVE || other.mode == Mode::END))
			           || (mode == Mode::READS && (other.mode == Mode::NATIVE || other.mode == Mode::END)) || (mode == Mode::NATIVE && other.mode == Mode::END)
			           || ((mode == other.mode) && ((mode == Mode::NATIVE) ? (native_iterator <= other.native_iterator) : true)
			                  && ((mode == Mode::WRITES) ? (writes_iterator <= other.writes_iterator) : true)
			                  && ((mode == Mode::READS) ? (reads_iterator <= other.reads_iterator) : true)));
		}

		bool operator<(const Iterator& other) const { return (*this) != other && (*this) <= other; }

		bool operator>=(const Iterator& other) const { return other <= (*this); }

		bool operator>(const Iterator& other) const { return (*this) != other && (*this) <= other; }
	};

	class MutableIterator : public Iterator
	{
	public:
		MutableIterator(Transaction& t, Mode m)
		 : Iterator(t, m)
		{
		}

		MutableIterator(Transaction& t, key_type k)
		 : Iterator(t, k)
		{
		}

		pair<key_type, Accessor> operator*(void)
		{
			const key_type key = this->current_key();
			return pair(move(key), this->trans[key]);
		}
	};

	class ConstIterator : public Iterator
	{
	public:
		ConstIterator(Transaction& t, Mode m)
		 : Iterator(t, m)
		{
		}

		ConstIterator(Transaction& t, key_type k)
		 : Iterator(t, k)
		{
		}

		pair<key_type, mapped_type> operator*(void)
		{
			const key_type key = this->current_key();
			return pair(move(key), this->trans[key]);
		}
	};

public:
	typedef MutableIterator iterator;
	typedef ConstIterator const_iterator;

	Transaction(Map& m, SharedMutex& a)
	 : back_map(m)
	 , access_mutex(a)
	{
		// reads.reserve(8);
		// writes.reserve(4);
		// counts.reserve(8);
		// erases.reserve(1);
	}

	Transaction(Map& m, ReadLockable<SharedMutex>& r)
	 : back_map(m)
	 , access_mutex(r)
	{
		// reads.reserve(8);
		// writes.reserve(4);
		// counts.reserve(8);
		// erases.reserve(1);
	}

	iterator begin(void) { return MutableIterator(*this, Mode::WRITES); }

	iterator end(void) { return MutableIterator(*this, Mode::END); }

	const_iterator cbegin(void) { return ConstIterator(*this, Mode::WRITES); }

	const_iterator cend(void) { return ConstIterator(*this, Mode::END); }

	iterator find(key_type key) { return MutableIterator(*this, key); }

	const_iterator find(key_type key) const { return ConstIterator(*this, key); }

	size_t count(key_type key)
	{
		if(writes.count(key))
		{
			return 1;
		}
		else if(erases.count(key))
		{
			return 0;
		}
		else if(counts.count(key))
		{
			return counts[key] ? 1 : 0;
		}
		else
		{
			SharedLock<SharedMutex> lock(access_mutex);
			return counts[key] = back_map.count(key);
		}
	}

	size_t size(void) { return back_map.size() - erases.size() + writes.size(); }

	Accessor operator[](key_type key) { return Accessor(*this, key); }

	template <typename Test>
	void commit_transaction(Test&& test)
	{
		InternalMap<key_type, mapped_type> writes_unwind;
		InternalSet<key_type> erases_unwind;

		for(const pair<key_type, mapped_type>& key_value : writes)
		{
			unique_lock<SharedMutex> lock(access_mutex.write_lockable());
			back_map[key_value.first] = key_value.second;
		}

		for(key_type key : erases)
		{
			unique_lock<SharedMutex> lock(access_mutex.write_lockable());
			back_map.erase(key);
		}
		
		Transaction tester(back_map, access_mutex);
		
		if(!test(tester))
		{
			unique_lock<SharedMutex> lock(access_mutex.write_lockable());

			for(const pair<key_type, mapped_type>& key_value : writes_unwind)
				back_map[key_value.first] = key_value.second;

			for(key_type key : erases_unwind)
				back_map.erase(key);

			throw TransactionError("Transaction requirements are not met.");
		}
	}

	~Transaction(void) {}
};
}

#ifdef DEBUG

#include <shared_mutex>
#include <algorithm>

namespace Logical
{

using std::chrono::milliseconds;
using std::shared_mutex;
using std::this_thread::sleep_for;
using std::none_of;
using std::unordered_map;
//using std::unordered_set;
using std::vector;


template<typename Collection>
static inline bool none_of(Collection&& collection)
{
	return none_of(collection.begin(), collection.end(), [](bool e){ return e; });
}

static inline bool none_of(initializer_list<bool>&& collection)
{
	return none_of<initializer_list<bool>>(move(collection));
}

static inline void sync_test_locks(void)
{
	auto access = shared_mutex();

	atomic_bool thread1_running(false);
	auto thread1 = Thread([&](void) {
		thread1_running = true;

		auto access_r = ReadLockable<decltype(access)>(access);

		sleep_for(milliseconds(100));
		access_r.lock();
		sleep_for(milliseconds(1000));
		access_r.unlock();
		sleep_for(milliseconds(100));

		bool locked = access_r.try_lock();
		logical_assert(locked, "Could not lock ReadLockable (thread 1).");
		sleep_for(milliseconds(100));
		access_r.unlock();
		sleep_for(milliseconds(100));

		thread1_running = false;
	});

	atomic_bool thread2_running(false);
	auto thread2 = Thread([&](void) {
		thread2_running = true;

		auto access_r = ReadLockable<decltype(access)>(access);

		sleep_for(milliseconds(300));
		access_r.lock();
		sleep_for(milliseconds(1000));
		access_r.unlock();
		sleep_for(milliseconds(100));

		bool locked = access_r.try_lock();
		logical_assert(locked, "Could not lock ReadLockable (thread 2).");
		sleep_for(milliseconds(100));
		access_r.unlock();
		sleep_for(milliseconds(100));

		thread2_running = false;
	});

	atomic_bool thread3_running(false);
	auto thread3 = Thread([&](void) {
		thread3_running = true;

		sleep_for(milliseconds(500));
		bool locked = access.try_lock();
		logical_assert(!locked, "It should not be possible to lock the mutex in exclusive mode (thread 3).");

		thread3_running = false;
	});

	auto guardian_thread = Thread([&](void) {
		sleep_for(milliseconds(4000));
		logical_assert(none_of({thread1_running, thread2_running, thread3_running}), "Threads using locks still running, possible deadlock.");
	});

	Thread::finalize({thread1, thread2, thread3, guardian_thread});
}

struct SyncTestError : public Error
{
public:
	SyncTestError(void)
	 : Error("Sync test error.")
	{
	}
};

static inline void sync_test_exceptions_1(void)
{
	auto thread1 = Thread([&](void) {
		sleep_for(milliseconds(500));
		throw SyncTestError();
		sleep_for(milliseconds(500));
	});
	
	try
	{
		Thread::finalize({thread1});
		logical_assert(false, "Exception should be thrown from the thread.");
	}
	catch(const SyncTestError& error)
	{
	}
}

static inline void sync_test_exceptions_2(void)
{
	auto thread1 = Thread([&](void) {
		sleep_for(milliseconds(500));
		throw SyncTestError();
		sleep_for(milliseconds(500));
	});
	
	auto thread2 = Thread([&](void) {
		sleep_for(milliseconds(5000));
	});
	
	try
	{
		Thread::finalize({thread1, thread2});
		logical_assert(false, "Exception should be thrown from the thread.");
	}
	catch(const SyncTestError& error)
	{
	}
}



static inline void sync_test_transaction_1(void)
{
	shared_mutex table_mutex;
	unordered_map<size_t, size_t> table;
	table.reserve(100);
	
	for(size_t i = 0; i < 100; i++)
		table[i] = i;
	
	const size_t max_failures = 5;
	size_t failures = 0;
	while(true)
	{
		try
		{
			Transaction<unordered_map<size_t, size_t>, shared_mutex> store(table, table_mutex);
			
			for(size_t i = 0; i < 100; i++)
				if(i % 2 == 1)
					store[i] = store[i - 1] + 2;
			
			store.commit_transaction([](auto& store)->bool
			{
				for(size_t i = 0; i < 100; i++)
				{
					if(i % 2 == 1)
					{
						if(!(store[i] == store[i - 1] + 2))
							return false;
					}
					else
					{
						if(!(store[i] == i))
							return false;
					}
				}
				return true;
			});
			break;
		}
		catch(const TransactionError& htte)
		{
			if(++failures >= max_failures)
				throw htte;
		}
	}

}


static inline void sync_test_transaction_2(void)
{
	shared_mutex table_mutex;
	unordered_map<size_t, size_t> table;
	table.reserve(110);
	
	for(size_t i = 0; i < 110; i++)
		table[i] = i;
	
	const size_t max_failures = 10;
	
	const auto task = [&](const size_t j)
	{
		size_t failures = 0;
		while(true)
		{
			try
			{
				Transaction<unordered_map<size_t, size_t>, shared_mutex> store(table, table_mutex);
				
				for(size_t i = 10 * j; i < 10 * (j + 1) + 10; i++)
					store[i] = j;
				
				store.commit_transaction([j](auto& store)->bool
				{
					for(size_t i = 10 * j; i < 10 * (j + 1) + 10; i++)
						if(!(store[i] == j))
							return false;
					return true;
				});
				
				break;
			}
			catch(const TransactionError& htte)
			{
				if(++failures >= max_failures)
					throw htte;
			}
		}
	};
	
	vector<Thread> threads;
	threads.reserve(10);
	
	for(size_t j = 0; j < 10; j++)
		threads.push_back(Thread(task, (size_t)j));
	
	Thread::finalize(threads);
}


static inline void sync_test(void)
{
	cout << " sync_test_locks" << endl;
	sync_test_locks();
	cout << " sync_test_exceptions_1" << endl;
	sync_test_exceptions_1();
	cout << " sync_test_exceptions_2" << endl;
	sync_test_exceptions_2();
	cout << " sync_test_transaction_1" << endl;
	sync_test_transaction_1();
	cout << " sync_test_transaction_2" << endl;
	sync_test_transaction_2();
}

}

#endif // DEBUG

#endif // LOGICAL_SYNC_HH
