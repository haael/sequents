

#ifndef LOGICAL_COLLECTIONS_HH
#define LOGICAL_COLLECTIONS_HH

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <exception>
#include <mutex>
#include <thread>
#include <type_traits>
#include <unordered_set>
#include <utility>
#include <vector>
#include <iterator>
#include <tuple>
#include <iostream>

#include "errors.hh"
#include "logical.hh"
#include "sync.hh"
#include "utils.hh"

namespace Logical
{

using std::atomic_bool;
using std::atomic_size_t;
using std::condition_variable;
using std::count_if;
using std::current_exception;
using std::exception_ptr;
using std::is_same;
using std::lock_guard;
using std::mutex;
using std::pair;
using std::rethrow_exception;
using std::sort;
using std::thread;
using std::unique;
using std::unique_lock;
using std::unordered_set;
using std::vector;
using std::random_access_iterator_tag;
using std::remove_reference;
using std::remove_cv;
using std::tie;
using std::ostream;
using chrono_milliseconds = std::chrono::milliseconds;
static constexpr auto get_this_thread_id = std::this_thread::get_id;

static inline size_t min_size(size_t a, size_t b) { return (a < b) ? a : b; }


template <typename Collection1, typename Collection2, typename Compare>
bool collections_equal(const Collection1& collection1, const Collection2& collection2, const Compare& elements_equal)
{
	if(collection1.size() != collection2.size())
		return false;

	const size_t s = collection1.size();
	for(size_t i = 0; i < s; i++)
		if(!elements_equal(collection1[i], collection2[i]))
			return false;

	return true;
}

template <typename Collection>
class Iterator
{
private:
	const Collection& collection;
	size_t index;

public:
	typedef decltype(collection[0]) item_type;
	typedef random_access_iterator_tag iterator_category;
	typedef typename Collection::value_type value_type;
	typedef ptrdiff_t difference_type;
	typedef value_type* pointer;
	typedef value_type& reference;

	Iterator(const Collection& col, size_t idx)
	 : collection(col)
	 , index(idx)
	{
	}

	item_type operator* (void) const { return collection[index]; }

	item_type operator[] (ptrdiff_t offset) const { return collection[index + offset]; }

	item_type operator-> (void) const { return collection[index]; }

	Iterator& operator++(void)
	{
		index++;
		return *this;
	}

	Iterator& operator--(void)
	{
		index--;
		return *this;
	}

	Iterator& operator+=(ptrdiff_t offset)
	{
		index += offset;
		return *this;
	}

	Iterator& operator-=(ptrdiff_t offset)
	{
		index -= offset;
		return *this;
	}

	Iterator operator+(ptrdiff_t offset) const { return Iterator(collection, index + offset); }

	Iterator operator-(ptrdiff_t offset) const { return Iterator(collection, index - offset); }
	
	ptrdiff_t operator-(const Iterator& other) const
	{
		if(&collection != &other.collection)
			throw IteratorError("Can not take a difference of iterators of different object", index, (void*)&collection, (void*)&other.collection);
		return index - other.index;
	}
	
	bool operator==(const Iterator& other) const { return (&collection == &other.collection) && (index == other.index); }

	bool operator!=(const Iterator& other) const { return !(*this == other); }

	bool operator<(const Iterator& other) const
	{
		if(&collection != &other.collection)
			throw IteratorError("Iterators point to different collections.", index, &collection, &other.collection);
		return index < other.index;
	}

	bool operator<=(const Iterator& other) const { return (*this < other) || (*this == other); }

	bool operator>(const Iterator& other) const { return !(*this <= other); }

	bool operator>=(const Iterator& other) const { return !(*this < other); }
};

extern volatile atomic_size_t max_thread_count;
extern volatile sig_atomic_t thread_error;
static const unsigned long wakeup_every_ms = 4000;


template<class T> struct remove_reference_only     { };
template<class T> struct remove_reference_only<T&> { typedef T type; };


template <typename Collection>
class Parallel
{
private:
	Collection collection;

public:
	typedef decltype(collection[0]) item_type;
	typedef typename Collection::value_type value_type;

	Parallel(void) = delete;
 
	template<typename CollectionI>
	Parallel(CollectionI&& col)
	 : collection(forward<CollectionI>(col))
	{
	}

	Parallel(const Parallel& cp)
	 : collection(cp.collection)
	{
	}

	Parallel(Parallel&& mv)
	 : collection(move(mv.collection))
	{
	}

	size_t size(void) const { return collection.size(); }

	item_type operator[](const size_t index) const { return collection[index]; }

	Iterator<Parallel> begin(void) const { return Iterator<Parallel>(*this, 0); }

	Iterator<Parallel> end(void) const { return Iterator<Parallel>(*this, size()); }

	template<typename ItemType>
	auto forward_element(const typename remove_reference_only<ItemType>::type& element) const
	{
		return reference_wrapper(element);
	}
	
	template<typename ItemType>
	auto forward_element(const pair<typename ItemType::first_type, typename ItemType::second_type>& element) const
	{
		return element;
	}
	
	template <typename Callable>
	bool run_parallel(const bool mode, const Callable& task) const
	{
		mutex count_mutex;
		condition_variable count_condition;

		atomic_bool result(!mode);

		vector<Thread> threads;
		threads.reserve(max_thread_count ? size() : min_size(max_thread_count, size()));

		//for(item_type element : collection)
		//{
		//	std::cerr << "thread to spawn @" << (const void*)(&element) << std::endl;
		//}

		for(item_type element : collection)
		{
			if(!(result != mode && !thread_error))
				break;
			
			unique_lock<mutex> count_lock(count_mutex);
			
			if(max_thread_count)
			{
				while(!count_condition.wait_for(count_lock, chrono_milliseconds(wakeup_every_ms), [&]()
				{
					return !(max_thread_count && count_if(threads.begin(), threads.end(), [](Thread &thr) -> bool { return thr.running(); }) >= max_thread_count);
				}))
					if(thread_error)
						break;

				auto thr_it = threads.begin();
				while(thr_it != threads.end())
				{
					Thread& thr = *thr_it;
					if(thr.running())
					{
						thr_it++;
					}
					else
					{
						if(thr.joinable())
							thr.join();
						thr_it = threads.erase(thr_it);
					}
				}
			}

			//std::cerr << "spawning thread @" << (const void*)(&element) << std::endl;
			
			threads.push_back(Thread(
			    [&](const value_type& element) {
				    exception_ptr exception = nullptr;

				    try
				    {
					    const bool task_result = task(element);

					    if(mode)
						    result = result | task_result;
					    else
						    result = result & task_result;
				    }
				    catch(...)
				    {
					    result = mode;
					    exception = current_exception();
				    }

				    if(max_thread_count)
					{
						unique_lock<mutex> count_lock(count_mutex);
						count_lock.unlock();
					    count_condition.notify_one();
					}

				    if(exception)
					    rethrow_exception(exception);
			    },
				forward_element<item_type>(element)));
		}
		
		Thread::finalize(threads);
		
		return result;
	}

	template <typename Callable>
	bool for_all(const Callable& task) const
	{
		return run_parallel(false, task);
	}

	template <typename Callable>
	bool for_any(const Callable& task) const
	{
		return run_parallel(true, task);
	}

	template <typename Callable>
	Reorder<Collection> sort(const Callable& weight) const
	{
		return Reorder<Collection>(collection).sort(weight);
	}

	template <typename Callable>
	Reorder<Collection> sort_unique(const Callable& weight) const
	{
		return Reorder<Collection>(collection).sort_unique(weight);
	}
};


/*
template<typename PairType1, typename PairType2>
auto Parallel::forward_element<pair<PairType1, PairType2>>(const pair<PairType1, PairType2>& element) const
{
	return element;
}
*/



/*
template <typename Collection, typename Predicate>
class Filter
{
private:
	const Collection& collection;
	const Predicate& predicate;

public:
	typedef decltype(collection[0]) item_type;
	typedef typename Collection::value_type value_type;
	
	Filter(const Collection& c, const Predicate& p)
	 : collection(c), predicate(p)
	{
	}
	
	item_type operator[](const size_t index) const { return collection[order[index]]; }
	
};*/


template <typename Collection>
class Reorder
{
private:
	Collection collection;
	vector<size_t> order;

	typedef pair<size_t, float> weight_record;

public:
	typedef decltype(collection[0]) item_type;
	typedef typename Collection::value_type value_type;

	Reorder(void) = delete;

	Reorder(const Collection& col)
	 : collection(col)
	{
		order.reserve(collection.size());
		for(size_t i = 0; i < collection.size(); i++)
			order.push_back(i);
	}

	Reorder(Collection&& col)
	 : collection(move(col))
	{
		order.reserve(collection.size());
		for(size_t i = 0; i < collection.size(); i++)
			order.push_back(i);
	}

	Reorder(const Reorder& cp)
	 : collection(cp.collection), order(cp.order)
	{
	}

	Reorder(Reorder&& mv)
	 : collection(move(mv.collection)), order(move(mv.order))
	{
	}

	size_t size(void) const { return order.size(); }

	item_type operator[](const size_t index) const { return collection[order[index]]; }

	Iterator<Reorder> begin(void) const { return Iterator<Reorder>(*this, 0); }

	Iterator<Reorder> end(void) const { return Iterator<Reorder>(*this, size()); }

	template <typename Callable>
	Reorder& sort(const Callable& weight)
	{
		vector<weight_record> weights;
		weights.reserve(collection.size());
		for(size_t i = 0; i < collection.size(); i++)
			weights.push_back(weight_record(i, weight(collection[i])));

		std::sort(weights.begin(), weights.end(), [](const weight_record& one, const weight_record& two) -> bool { return one.second < two.second; });

		order.clear();
		order.reserve(weights.size());
		for(const auto& w : weights)
			order.push_back(w.first);

		return *this;
	}

	template <typename Callable>
	Reorder& sort_unique(const Callable& weight)
	{
		vector<weight_record> weights;
		weights.reserve(collection.size());
		for(size_t i = 0; i < collection.size(); i++)
			weights.push_back(weight_record(i, weight(collection[i])));

		std::sort(weights.begin(), weights.end(), [](const weight_record& one, const weight_record& two) -> bool { return one.second < two.second; });
		auto past_end
		    = unique(weights.begin(), weights.end(), [](const weight_record& one, const weight_record& two) -> bool { return one.second == two.second; });
		weights.erase(past_end, weights.end());

		order.clear();
		order.reserve(weights.size());
		for(const auto& w : weights)
			order.push_back(w.first);

		return *this;
	}

	template <typename Callable>
	bool run_parallel(const bool mode, const Callable& task) const
	{
		return Parallel<Reorder>(*this).run_parallel(mode, task);
	}

	template <typename Callable>
	bool for_all(const Callable& task) const
	{
		return Parallel<Reorder>(*this).for_all(task);
	}

	template <typename Callable>
	bool for_any(const Callable& task) const
	{
		return Parallel<Reorder>(*this).for_any(task);
	}
};

template <typename Collection1, typename Collection2>
class Concat
{
	static_assert(is_same<typename Collection1::value_type, typename Collection2::value_type>::value, "Item types of both arguments must be the same.");

private:
	Collection1 one;
	Collection2 two;

public:
	typedef decltype(one[0]) item_type;
	typedef typename Collection1::value_type value_type;

	Concat(void) = delete;

	Concat(const Collection1& p_one, const Collection2& p_two)
	 : one(p_one)
	 , two(p_two)
	{
	}

	Concat(const Concat& cp)
	 : one(cp.one)
	 , two(cp.two)
	{
	}

	Concat(Concat&& mv)
	 : one(move(mv.one))
	 , two(move(mv.two))
	{
	}

	size_t size(void) const { return one.size() + two.size(); }

	size_t count(const value_type& v) const { return one.count(v) + two.count(v); }

	item_type operator[](const size_t index) const
	{
		if(index < one.size())
			return one[index];
		else if(index < one.size() + two.size())
			return two[index - one.size()];
		else
			throw IndexError("Index out of range in Concat collection.", index, size());
	}

	Iterator<Concat> begin(void) const { return Iterator<Concat>(*this, 0); }

	Iterator<Concat> end(void) const { return Iterator<Concat>(*this, size()); }

	template <typename CollectionA>
	Concat<Concat, CollectionA> operator+(const CollectionA& that) const
	{
		return Concat<Concat, CollectionA>(*this, that);
	}

	template <typename CollectionS>
	Difference<Concat, CollectionS> operator-(const CollectionS& that) const
	{
		return Difference<Concat, CollectionS>(*this, that);
	}

	template <typename CollectionF>
	Cartesian<Concat, CollectionF> operator*(const CollectionF& that) const
	{
		return Cartesian<Concat, CollectionF>(*this, that);
	}

	template <typename Callable>
	Reorder<Concat> sort(const Callable& weight)
	{
		return Reorder<Concat>(*this).sort(weight);
	}

	template <typename Callable>
	Reorder<Concat> sort_unique(const Callable& weight)
	{
		return Reorder<Concat>(*this).sort_unique(weight);
	}

	template <typename Callable>
	bool run_parallel(const bool mode, const Callable& task) const
	{
		return Parallel<Concat>(*this).run_parallel(mode, task);
	}

	template <typename Callable>
	bool for_all(const Callable& task) const
	{
		return Parallel<Concat>(*this).for_all(task);
	}

	template <typename Callable>
	bool for_any(const Callable& task) const
	{
		return Parallel<Concat>(*this).for_any(task);
	}
};

template <typename Collection1, typename Collection2>
class Difference
{
	static_assert(is_same<typename Collection1::value_type, typename Collection2::value_type>::value, "Item types of both arguments must be the same.");

private:
	Collection1 one;
	Collection2 two;

public:
	typedef decltype(one[0]) item_type;
	typedef typename Collection1::value_type value_type;

	Difference(void) = delete;

	Difference(const Collection1& p_one, const Collection2& p_two)
	 : one(p_one)
	 , two(p_two)
	{
	}

	Difference(const Difference& cp)
	 : one(cp.one)
	 , two(cp.two)
	{
	}

	Difference(Difference&& mv)
	 : one(move(mv.one))
	 , two(move(mv.two))
	{
	}

	size_t size(void) const
	{
		return count_if(one.begin(), one.end(), [this](const value_type& item) -> bool { return !two.count(item); });
	}

	size_t count(const value_type& item) const
	{
		if(two.count(item)) return 0;
		else return one.count(item);
	}

	item_type operator[](const size_t index) const
	{
		size_t skip = 0;
		size_t shift = 0;
		auto begin_iter = one.begin();
		auto end_iter = one.end();
		do
		{
			shift++;
			skip = count_if(begin_iter, begin_iter + shift, [this](const value_type& item) -> bool { return !two.count(item); });
		} while(begin_iter + shift != end_iter && skip <= index);

		if(skip <= index)
			throw IndexError("Element not found for the provided index in Difference collection.", index, size());

		return one[shift - 1];
	}

	Iterator<Difference> begin(void) const { return Iterator<Difference>(*this, 0); }

	Iterator<Difference> end(void) const { return Iterator<Difference>(*this, size()); }

	template <typename CollectionA>
	Concat<Difference, CollectionA> operator+(const CollectionA& that) const
	{
		return Concat<Difference, CollectionA>(*this, that);
	}

	template <typename CollectionS>
	Difference<Difference, CollectionS> operator-(const CollectionS& that) const
	{
		return Difference<Difference, CollectionS>(*this, that);
	}

	template <typename CollectionF>
	Cartesian<Difference, CollectionF> operator*(const CollectionF& that) const
	{
		return Cartesian<Difference, CollectionF>(*this, that);
	}

	template <typename Callable>
	bool run_parallel(const bool mode, const Callable& task) const
	{
		return Parallel<Difference>(*this).run_parallel(mode, task);
	}

	template <typename Callable>
	bool for_all(const Callable& task) const
	{
		return Parallel<Difference>(*this).for_all(task);
	}

	template <typename Callable>
	bool for_any(const Callable& task) const
	{
		return Parallel<Difference>(*this).for_any(task);
	}

	template <typename Callable>
	Reorder<Difference> sort(const Callable& weight) const
	{
		return Reorder<Difference>(*this).sort(weight);
	}

	template <typename Callable>
	Reorder<Difference> sort_unique(const Callable& weight) const
	{
		return Reorder<Difference>(*this).sort_unique(weight);
	}
};

template <typename Item>
class Empty
{
public:
	typedef const Item& item_type;
	typedef Item value_type;
	
	Empty(const initializer_list<Item>& e)
	{
		logical_assert(e.size() == 0, "Initializer list of the Empty sequence must be empty.");
	}
	
	Empty(void) {}

	size_t size(void) const { return 0; }

	item_type operator[](const size_t index) const { throw IndexError("Trying to get element from Empty collection.", index, size()); }

	size_t count(const value_type& item_p) const
	{
		return 0;
	}

	template <typename Equal>
	size_t count(const value_type& item_p, const Equal& equal) const
	{
		return 0;
	}

	Iterator<Empty> begin(void) const { return Iterator<Empty>(*this, 0); }

	Iterator<Empty> end(void) const { return Iterator<Empty>(*this, size()); }

	template <typename CollectionA>
	Concat<Empty, CollectionA> operator+(const CollectionA& that) const
	{
		return Concat<Empty, CollectionA>(*this, that);
	}

	template <typename CollectionS>
	Difference<Empty, CollectionS> operator-(const CollectionS& that) const
	{
		return Difference<Empty, CollectionS>(*this, that);
	}

	template <typename CollectionF>
	Cartesian<Empty, CollectionF> operator*(const CollectionF& that) const
	{
		return Cartesian<Empty, CollectionF>(*this, that);
	}

	template <typename Callable>
	bool run_parallel(const bool mode, const Callable& task) const
	{
		return Parallel<Empty>(*this).run_parallel(mode, task);
	}

	template <typename Callable>
	bool for_all(const Callable& task) const
	{
		return Parallel<Empty>(*this).for_all(task);
	}

	template <typename Callable>
	bool for_any(const Callable& task) const
	{
		return Parallel<Empty>(*this).for_any(task);
	}

	template <typename Callable>
	Reorder<Empty> sort(const Callable& weight) const
	{
		return Reorder<Empty>(*this).sort(weight);
	}

	template <typename Callable>
	Reorder<Empty> sort_unique(const Callable& weight) const
	{
		return Reorder<Empty>(*this).sort_unique(weight);
	}
};



template <typename Item>
class Singleton
{
private:
	const Item& item;

public:
	typedef const Item& item_type;
	typedef Item value_type;

	Singleton(void) = delete;

	Singleton(const Item& it)
	 : item(it)
	{
	}

	size_t size(void) const { return 1; }

	item_type operator[](const size_t index) const
	{
		if(index != 0)
			throw IndexError("Nonzero index in Singleton collection.", index, size());
		return item;
	}

	size_t count(const value_type& item_p) const
	{
		return count(item_p, [](const value_type& one, const value_type& two) -> bool { return &one == &two; });
	}

	template <typename Equal>
	size_t count(const value_type& item_p, const Equal& equal) const
	{
		if(equal(item, item_p))
			return 1;
		else
			return 0;
	}

	Iterator<Singleton> begin(void) const { return Iterator<Singleton>(*this, 0); }

	Iterator<Singleton> end(void) const { return Iterator<Singleton>(*this, size()); }

	template <typename CollectionA>
	Concat<Singleton, CollectionA> operator+(const CollectionA& that) const
	{
		return Concat<Singleton, CollectionA>(*this, that);
	}

	template <typename CollectionS>
	Difference<Singleton, CollectionS> operator-(const CollectionS& that) const
	{
		return Difference<Singleton, CollectionS>(*this, that);
	}

	template <typename CollectionF>
	Cartesian<Singleton, CollectionF> operator*(const CollectionF& that) const
	{
		return Cartesian<Singleton, CollectionF>(*this, that);
	}

	template <typename Callable>
	bool run_parallel(const bool mode, const Callable& task) const
	{
		return Parallel<Singleton>(*this).run_parallel(mode, task);
	}

	template <typename Callable>
	bool for_all(const Callable& task) const
	{
		return Parallel<Singleton>(*this).for_all(task);
	}

	template <typename Callable>
	bool for_any(const Callable& task) const
	{
		return Parallel<Singleton>(*this).for_any(task);
	}

	template <typename Callable>
	Reorder<Singleton> sort(const Callable& weight) const
	{
		return Reorder<Singleton>(*this).sort(weight);
	}

	template <typename Callable>
	Reorder<Singleton> sort_unique(const Callable& weight) const
	{
		return Reorder<Singleton>(*this).sort_unique(weight);
	}
};

template <typename Collection>
class Shadow
{
private:
	const Collection& collection;

public:
	typedef decltype(collection[0]) item_type;
	typedef typename Collection::value_type value_type;

	Shadow(void) = delete;

	Shadow(const Collection& p_collection)
	 : collection(p_collection)
	{
	}

	size_t size(void) const { return collection.size(); }

	item_type operator[](const size_t index) const
	{
		if(index >= size())
			throw IndexError("Index out of range in Shadow collection.", index, size());
		return collection[index];
	}

	size_t count(const value_type& item_p) const
	{
		return count(item_p, [](const value_type& one, const value_type& two) -> bool { return &one == &two; });
	}

	template <typename Equal>
	size_t count(const value_type& item_p, const Equal& equal) const
	{
		size_t c = 0;
		for(const auto& item : *this)
			if(equal(item, item_p))
				c++;
		return c;
	}

	Iterator<Shadow> begin(void) const { return Iterator<Shadow>(*this, 0); }

	Iterator<Shadow> end(void) const { return Iterator<Shadow>(*this, size()); }

	template <typename CollectionA>
	Concat<Shadow, CollectionA> operator+(const CollectionA& that) const
	{
		return Concat<Shadow, CollectionA>(*this, that);
	}

	template <typename CollectionS>
	Difference<Shadow, CollectionS> operator-(const CollectionS& that) const
	{
		return Difference<Shadow, CollectionS>(*this, that);
	}

	template <typename CollectionF>
	Cartesian<Shadow, CollectionF> operator*(const CollectionF& that) const
	{
		return Cartesian<Shadow, CollectionF>(*this, that);
	}

	template <typename Callable>
	bool run_parallel(const bool mode, const Callable& task) const
	{
		return Parallel<Shadow>(*this).run_parallel(mode, task);
	}

	template <typename Callable>
	bool for_all(const Callable& task) const
	{
		return Parallel<Shadow>(*this).for_all(task);
	}

	template <typename Callable>
	bool for_any(const Callable& task) const
	{
		return Parallel<Shadow>(*this).for_any(task);
	}

	template <typename Callable>
	Reorder<Shadow> sort(const Callable& weight) const
	{
		return Reorder<Shadow>(*this).sort(weight);
	}

	template <typename Callable>
	Reorder<Shadow> sort_unique(const Callable& weight) const
	{
		return Reorder<Shadow>(*this).sort_unique(weight);
	}
};


template <typename Item>
class Unfold
{
public:
	typedef Item value_type;
	typedef const Item& item_type;

private:
	const value_type** items;
	size_t item_count;

public:
	Unfold(void)
	 : items(nullptr), item_count(0)
	{
	}

	template<typename Collection>
	Unfold(const Collection& col)
	 : items(nullptr), item_count(col.size())
	{
		items = new const value_type*[item_count];
		size_t i = 0;
		for(const value_type& v : col) // FIXME: correct?
			items[i++] = &v;
	}

	Unfold(const Unfold& col)
	 : items(nullptr), item_count(col.size())
	{
		items = new const value_type*[item_count];
		size_t i = 0;
		for(const value_type& v : col) // FIXME: correct?
			items[i++] = &v;
	}

	Unfold(Unfold&& col)
	 : items(col.items), item_count(col.size())
	{
		col.items = nullptr;
	}

	~Unfold(void)
	{
		if(items) delete[] items;
	}

	size_t size(void) const { return item_count; }

	item_type operator[](const size_t index) const
	{
		if(index >= size())
			throw IndexError("Index out of range in Shadow collection.", index, size());
		return *items[index];
	}

	size_t count(const value_type& item_p) const
	{
		return count(item_p, [](const value_type& one, const value_type& two) -> bool { return &one == &two; });
	}

	template <typename Equal>
	size_t count(const value_type& item_p, const Equal& equal) const
	{
		size_t c = 0;
		for(const auto& item : *this)
			if(equal(item, item_p))
				c++;
		return c;
	}

	Iterator<Unfold> begin(void) const { return Iterator<Unfold>(*this, 0); }

	Iterator<Unfold> end(void) const { return Iterator<Unfold>(*this, size()); }

	template <typename CollectionA>
	Concat<Unfold, CollectionA> operator+(const CollectionA& that) const
	{
		return Concat<Unfold, CollectionA>(*this, that);
	}

	template <typename CollectionS>
	Difference<Unfold, CollectionS> operator-(const CollectionS& that) const
	{
		return Difference<Unfold, CollectionS>(*this, that);
	}

	template <typename CollectionF>
	Cartesian<Unfold, CollectionF> operator*(const CollectionF& that) const
	{
		return Cartesian<Unfold, CollectionF>(*this, that);
	}

	template <typename Callable>
	bool run_parallel(const bool mode, const Callable& task) const
	{
		return Parallel<Unfold>(*this).run_parallel(mode, task);
	}

	template <typename Callable>
	bool for_all(const Callable& task) const
	{
		return Parallel<Unfold>(*this).for_all(task);
	}

	template <typename Callable>
	bool for_any(const Callable& task) const
	{
		return Parallel<Unfold>(*this).for_any(task);
	}

	template <typename Callable>
	Reorder<Unfold> sort(const Callable& weight) const
	{
		return Reorder<Unfold>(*this).sort(weight);
	}

	template <typename Callable>
	Reorder<Unfold> sort_unique(const Callable& weight) const
	{
		return Reorder<Unfold>(*this).sort_unique(weight);
	}
	
	void print(ostream& out) const
	{
		out << "{ ";
		bool first = true;
		for(const auto& f : (*this))
		{
			if(!first)
				out << ", ";
			else
				first = false;
			out << f;
		}
		out << " }";
	}

	void print_addresses(ostream& out) const
	{
		out << "{ ";
		bool first = true;
		for(const auto& f : (*this))
		{
			if(!first)
				out << ", ";
			else
				first = false;
			out << (&f);
		}
		out << " }";
	}

	void print_with_addresses(ostream& out) const
	{
		out << "{ ";
		bool first = true;
		for(const auto& f : (*this))
		{
			if(!first)
				out << ", ";
			else
				first = false;
			out << f << " @" << (&f);
		}
		out << " }";
	}
};


template <typename Collection1, typename Collection2>
class Cartesian
{
private:
	Collection1 one;
	Collection2 two;

public:
	typedef const pair<const typename Collection1::value_type&, const typename Collection2::value_type&> item_type;
	typedef pair<const typename Collection1::value_type&, const typename Collection2::value_type&> value_type;

	Cartesian(void) = delete;

	template<typename Collection1I, typename Collection2I>
	Cartesian(Collection1I&& p_one, Collection2I&& p_two)
	 : one(forward<Collection1I>(p_one))
	 , two(forward<Collection2I>(p_two))
	{
	}

	Cartesian(const Cartesian& cp)
	 : one(cp.one)
	 , two(cp.two)
	{
	}

	Cartesian(Cartesian&& mv)
	 : one(move(mv.one))
	 , two(move(mv.two))
	{
	}

	size_t size(void) const { return one.size() * two.size(); }

	item_type operator[](const size_t index) const
	{
		const size_t i = index % one.size();
		const size_t j = index / one.size();
		return value_type(one[i], two[j]);
	}

	Iterator<Cartesian> begin(void) const { return Iterator<Cartesian>(*this, 0); }

	Iterator<Cartesian> end(void) const { return Iterator<Cartesian>(*this, size()); }

	template <typename Callable>
	Reorder<Cartesian> sort(const Callable& weight) const
	{
		return Reorder<Cartesian>(*this).sort(weight);
	}

	template <typename Callable>
	Reorder<Cartesian> sort_unique(const Callable& weight) const
	{
		return Reorder<Cartesian>(*this).sort_unique(weight);
	}

	template <typename Callable>
	bool run_parallel(const bool mode, const Callable& task) const
	{
		return Parallel<Cartesian>(*this).run_parallel(mode, task);
	}

	template <typename Callable>
	bool for_all(const Callable& task) const
	{
		return Parallel<Cartesian>(*this).for_all(task);
	}

	template <typename Callable>
	bool for_any(const Callable& task) const
	{
		return Parallel<Cartesian>(*this).for_any(task);
	}
};


template<typename Item>
inline ostream& operator<<(ostream& stream, const Unfold<Item>& f)
{
	f.print(stream);
	return stream;
}



} // namespace Logical

#ifdef DEBUG

#include <array>
#include <initializer_list>
#include <iostream>
#include <string>
#include <type_traits>
#include <iterator>
#include <sstream>

using std::array;
using std::cout;
using std::endl;
using std::initializer_list;
using std::ostream;
using std::string;
using std::is_same;
using std::iterator_traits;
using std::random_access_iterator_tag;
using std::stringstream;
using std::ostream;

namespace Logical
{


template <typename Assertion>
struct AssertValue
{
    static_assert(Assertion::value, "Assertion failed <see below for more information>");
    static bool const value = Assertion::value;
};

template <typename Type1, typename Type2>
struct AssertTypesSame
{
    static_assert(is_same<Type1, Type2>::value, "Assertion failed <see below for more information>");
    static bool const value = is_same<Type1, Type2>::value;
};

static_assert(AssertTypesSame< iterator_traits<Iterator<Empty<int>>>::value_type, int>::value);
static_assert(AssertValue<is_same<iterator_traits<Iterator<Singleton<int>>>::value_type, int>>::value);
static_assert(AssertValue<is_same<iterator_traits<Iterator<Shadow<array<int, 3>>>>::value_type, int>>::value);

static_assert(AssertValue<is_same<iterator_traits<Iterator<Shadow<vector<int>>>>::value_type, int>>::value);
static_assert(AssertValue<is_same<iterator_traits<Iterator<Shadow<vector<int>>>>::difference_type, ptrdiff_t>>::value);
static_assert(AssertValue<is_same<iterator_traits<Iterator<Shadow<vector<int>>>>::iterator_category, random_access_iterator_tag>>::value);

static inline int random_int(int seed) { return (seed + 1337) % 91; }

static inline vector<int> random_int_vector(int seed, size_t size)
{
	vector<int> v;
	v.reserve(size);
	for(size_t i = 0; i < size; i++)
		v.push_back(random_int(seed++));
	return v;
}

template <typename Collection1, typename Collection2>
inline void collections_concat_test(const Collection1& col1, const Collection2& col2)
{
	auto concat_1 = col1 + col2;

	logical_assert(concat_1.size() == col1.size() + col2.size(), "Wrong size.");

	for(size_t i = 0; i < col1.size(); i++)
		logical_assert(concat_1[i] == col1[i], "Wrong element.");

	for(size_t i = 0; i < col2.size(); i++)
		logical_assert(concat_1[col1.size() + i] == col2[i], "Wrong element.");

	try
	{
		auto g = concat_1[concat_1.size()];
		logical_assert(false, "Index >= 1 should raise IndexError from Concat collection.");
	}
	catch(const IndexError& ie)
	{
	}
}

struct test_item
{
	int x, y;
	
	test_item(int _x, int _y)
	 : x(_x), y(_y)
	{
	}
	
	test_item& operator = (const test_item& that)
	{
		x = that.x;
		y = that.y;
		return *this;
	}		
};

ostream& operator << (ostream& os, const test_item& it)
{
	os << "item(" << it.x << "," << it.y << ")@" << &it;
	return os;
}


struct int_triple
{
	typedef int value_type;
	typedef const int& item_type;
	
	const int a, b, c;
	
	int_triple(int _a, int _b, int _c)
	 : a(_a), b(_b), c(_c)
	{
	}

	size_t size(void) const
	{
		return 3;
	}
	
	const int& operator [] (size_t index) const
	{
		switch(index)
		{
		case 0: return a;
		case 1: return b;
		case 2: return c;
		default: throw IndexError("int_triple index >= 3", index, 3);
		}
	}

	Iterator<int_triple> begin(void) const { return Iterator<int_triple>(*this, 0); }

	Iterator<int_triple> end(void) const { return Iterator<int_triple>(*this, size()); }
};



static inline void collections_difference_test(void)
{

	const auto a = test_item(0, 1);
	const auto b = test_item(0, 2);
	const auto c = test_item(1, 1);
	const auto d = test_item(0, 1);
	const auto e = test_item(0, 2);
	const auto f = test_item(3, 1);
	const auto g = test_item(3, 2);
	
	const auto v1 = vector<test_item>({a, b, c, d});
	const auto v2 = Shadow<vector<test_item>>(v1);
	const auto v3 = Singleton<test_item>(v1[2]);
	const auto v4 = v2 - v3;
	
	std::cout << &c << std::endl;
	std::cout << Unfold<test_item>(v3) << std::endl;
	std::cout << Unfold<test_item>(v4) << std::endl;
	logical_assert(v4.size() == 3);
}


static inline void collections_address_test(void)
{
	const auto u0 = int_triple(1, 2, 3);
	const auto u = Unfold<int>(u0);
	
	//std::cout << u << std::endl;
	//u.print_addresses(std::cout);
	//std::cout << std::endl;

	const auto w = u.sort([](const int& x) -> float { return (float)x; });

	//std::cout << Unfold<int>(w) << std::endl;
	//Unfold<int>(w).print_addresses(std::cout);
	//std::cout << std::endl;

	const auto z0 = int_triple(4, 5, 6);
	const auto z = Unfold<int>(z0);

	mutex m;

/*
	logical_assert((u + z)
	.sort([](const int& x) -> float
	{
		return (float)x;
	})
	.for_all([&u, &z, &m](const int& x) -> bool
	{
		lock_guard<mutex> l(m);
		std::cout << &x << std::endl;
		return (u + z).count(x);
	}));
*/

/*
	std::cout << "serial" << std::endl;
	for(const auto x : (u * z))
	{
		std::cout << x.first << "," << x.second << std::endl;
	}
	std::cout << std::endl;

	std::cout << "serial, sorted" << std::endl;
	for(const auto x : (u * z).sort([](const pair<const int&, const int&> x) -> float
	{
		return (float)(x.first + x.second);
	}))
	{
		std::cout << x.first << "," << x.second << std::endl;
	}
	std::cout << std::endl;
*/

	vector<pair<int, int>> us;
	us.reserve((u * z).size());
	
	//std::cout << "parallel" << std::endl;
	(u * z)
	.sort([](const pair<const int&, const int&> x) -> float
	{
		return (float)(x.first + x.second);
	})
	.for_all([&u, &z, &m, &us](const pair<const int&, const int&> x) -> bool
	{
		lock_guard<mutex> l(m);
		//std::cout << x.first << "," << x.second << std::endl;
		us.push_back(pair(x.first, x.second));
		return true;
	});
	//std::cout << std::endl;
	
	logical_assert((u * z)
	.sort([](const pair<const int&, const int&> x) -> float
	{
		return (float)(x.first + x.second);
	})
	.for_all([&u, &z, &m, &us](const pair<const int&, const int&> x) -> bool
	{
		lock_guard<mutex> l(m);
		//std::cout << x.first << "," << x.second << std::endl;
		for(const auto& y : us)
			if(x.first == y.first && x.second == y.second)
				return true;
		return false;
	}));
}


static inline void collections_cartesian_test(void)
{
	const auto v1 = int_triple(1, 2, 3);

	const auto v2 = Unfold<int>(v1);
	logical_assert(&v1[0] == &v2[0]);
	logical_assert(&v1[1] == &v2[1]);
	logical_assert(&v1[2] == &v2[2]);
	
	const auto u1 = int_triple(2, 3, 4);
	
	const auto uv = Unfold<int>(u1) * v2;

	logical_assert(&uv[0 + 0 * 3].first == &u1[0] && &uv[0 + 0 * 3].second == &v2[0], string_format("0x%x != 0x%x || 0x%x != 0x%x", &uv[0 + 0 * 3].first, &u1[0], &uv[0 + 0 * 3].second, &v2[0]).c_str());
	logical_assert(&uv[1 + 0 * 3].first == &u1[1] && &uv[1 + 0 * 3].second == &v2[0], string_format("0x%x != 0x%x || 0x%x != 0x%x", &uv[1 + 0 * 3].first, &u1[1], &uv[1 + 0 * 3].second, &v2[0]).c_str());
	logical_assert(&uv[2 + 0 * 3].first == &u1[2] && &uv[2 + 0 * 3].second == &v2[0], string_format("0x%x != 0x%x || 0x%x != 0x%x", &uv[2 + 0 * 3].first, &u1[2], &uv[2 + 0 * 3].second, &v2[0]).c_str());

	logical_assert(&uv[0 + 1 * 3].first == &u1[0] && &uv[0 + 1 * 3].second == &v2[1], string_format("0x%x != 0x%x || 0x%x != 0x%x", &uv[0 + 1 * 3].first, &u1[0], &uv[0 + 1 * 3].second, &v2[1]).c_str());
	logical_assert(&uv[1 + 1 * 3].first == &u1[1] && &uv[1 + 1 * 3].second == &v2[1], string_format("0x%x != 0x%x || 0x%x != 0x%x", &uv[1 + 1 * 3].first, &u1[1], &uv[1 + 1 * 3].second, &v2[1]).c_str());
	logical_assert(&uv[2 + 1 * 3].first == &u1[2] && &uv[2 + 1 * 3].second == &v2[1], string_format("0x%x != 0x%x || 0x%x != 0x%x", &uv[2 + 1 * 3].first, &u1[2], &uv[2 + 1 * 3].second, &v2[1]).c_str());

	logical_assert(&uv[0 + 2 * 3].first == &u1[0] && &uv[0 + 2 * 3].second == &v2[2], string_format("0x%x != 0x%x || 0x%x != 0x%x", &uv[0 + 2 * 3].first, &u1[0], &uv[0 + 2 * 3].second, &v2[2]).c_str());
	logical_assert(&uv[1 + 2 * 3].first == &u1[1] && &uv[1 + 2 * 3].second == &v2[2], string_format("0x%x != 0x%x || 0x%x != 0x%x", &uv[1 + 2 * 3].first, &u1[1], &uv[1 + 2 * 3].second, &v2[2]).c_str());
	logical_assert(&uv[2 + 2 * 3].first == &u1[2] && &uv[2 + 2 * 3].second == &v2[2], string_format("0x%x != 0x%x || 0x%x != 0x%x", &uv[2 + 2 * 3].first, &u1[2], &uv[2 + 2 * 3].second, &v2[2]).c_str());
}


inline void collections_test(void)
{
	collections_cartesian_test();
	
	auto empty = Empty<int>();
	logical_assert(empty.size() == 0, "Empty collection should have size = 0.");

	for(size_t i = 0; i < 100; i++)
	{
		const int k = random_int(i);
		auto singleton_1 = Singleton<int>(k);

		logical_assert(singleton_1.size() == 1, "Singleton collection should have size = 1.");
		logical_assert(singleton_1[0] == k, "Wrong element returned from Singleton collection.");

		try
		{
			auto g = singleton_1[1];
			logical_assert(false, "Index >= 1 should raise IndexError from Singleton collection.");
		}
		catch(const IndexError& ie)
		{
		}

		try
		{
			auto g = singleton_1[-1];
			logical_assert(false, "Index -1 should raise IndexError from Singleton collection.");
		}
		catch(const IndexError& ie)
		{
		}
	}

	for(size_t i = 0; i < 100; i++)
	{
		const size_t s = 10;
		auto array_2 = array<int, s>();
		for(size_t j = 0; j < s; j++)
			array_2[j] = random_int(i + 11 * j);

		auto shadow_2 = Shadow<array<int, s>>(array_2);

		logical_assert(shadow_2.size() == s, "Collection size is wrong.");

		for(size_t j = 0; j < s; j++)
			logical_assert(shadow_2[j] == random_int(i + 11 * j), "Wrong element returned.");

		try
		{
			auto g = shadow_2[s + i];
			logical_assert(false, "Index >= size should raise IndexError from Singleton collection.");
		}
		catch(const IndexError& ie)
		{
		}

		try
		{
			auto g = shadow_2[-1];
			logical_assert(false, "Index -1 should raise IndexError from Shadow collection.");
		}
		catch(const IndexError& ie)
		{
		}
	}

	for(size_t i = 0; i < 100; i++)
	{
		auto vector_3 = vector<int>();
		vector_3.reserve(i);
		for(size_t j = 0; j < i; j++)
			vector_3.push_back(random_int(i + 11 * j));

		auto shadow_3 = Shadow<vector<int>>(vector_3);

		logical_assert(shadow_3.size() == i, "Collection size is wrong.");

		for(size_t j = 0; j < i; j++)
			logical_assert(shadow_3[j] == random_int(i + 11 * j), "Wrong element returned.");

		try
		{
			auto g = shadow_3[shadow_3.size() + i];
			logical_assert(false, "Index >= size should raise IndexError from Singleton collection.");
		}
		catch(const IndexError& ie)
		{
		}

		try
		{
			auto g = shadow_3[-1];
			logical_assert(false, "Index -1 should raise IndexError from Shadow collection.");
		}
		catch(const IndexError& ie)
		{
		}
	}

	// auto concat_1_1 = singleton_1 + singleton_1;
	// auto concat_1_2 = singleton_1 + singleton_2;
	// auto concat_1_3 = singleton_1 + shadow_3;

	// auto concat_2_1 = singleton_2 + singleton_1;
	// auto concat_2_2 = singleton_2 + singleton_2;
	// auto concat_2_3 = singleton_2 + shadow_3;

	// auto concat_3_1 = shadow_3 + singleton_1;
	// auto concat_3_2 = shadow_3 + singleton_2;
	// auto concat_3_3 = shadow_3 + shadow_3;

	int r = 98341;

	{
		const auto a1 = Empty<int>();
		const auto a2 = Empty<int>();
		collections_concat_test(a1, a2);
	}

	{
		const auto a1 = Empty<int>();
		const auto a2 = Singleton<int>(random_int(r++));
		collections_concat_test(a1, a2);
	}

	{
		const auto a1 = Singleton<int>(random_int(r++));
		const auto a2 = Singleton<int>(random_int(r++));
		collections_concat_test(a1, a2);
	}

	{
		const auto a1 = Empty<int>();
		const auto b = array<int, 3>{random_int(r++), random_int(r++), random_int(r++)};
		const auto a2 = Shadow<array<int, 3>>(b);
		collections_concat_test(a1, a2);
	}

	{
		const auto a1 = Singleton<int>(random_int(r++));
		const auto b = array<int, 3>{random_int(r++), random_int(r++), random_int(r++)};
		const auto a2 = Shadow<array<int, 3>>(b);
		collections_concat_test(a1, a2);
	}

	{
		const auto b1 = array<int, 3>{random_int(r++), random_int(r++), random_int(r++)};
		const auto a1 = Shadow<array<int, 3>>(b1);
		const auto b2 = array<int, 3>{random_int(r++), random_int(r++), random_int(r++)};
		const auto a2 = Shadow<array<int, 3>>(b2);
		collections_concat_test(a1, a2);
	}

	{
		const auto b1 = array<int, 4>{random_int(r++), random_int(r++), random_int(r++), random_int(r++)};
		const auto a1 = Shadow<array<int, 4>>(b1);
		const auto b2 = vector<int>{random_int(r++), random_int(r++)};
		const auto a2 = Shadow<vector<int>>(b2);
		collections_concat_test(a1, a2);
	}

	/*{
	    const auto a1 = Singleton<float>(0.1f);
	    const auto a2 = Singleton<float>(-0.24f);
	    collections_concat_test(a1, a2);
	}*/

	const auto b0 = Empty<int>();
	logical_assert(b0.size() == 0, "Wrong size.");
	logical_assert(b0.for_all([](int el) { return el < 10; }), "Wrong result of parallel computation.");

	const auto v1 = vector<int>{8, random_int(r++) % 10, random_int(r++) % 10, random_int(r++) % 10, random_int(r++) % 10};
	const auto b1 = Shadow<vector<int>>(v1);
	logical_assert(b1.size() == v1.size(), "Wrong size.");
	logical_assert(b1.for_all([](int el) { return el < 10; }), "Wrong result of parallel computation.");

	const auto v2 = random_int_vector(r++, 10000);
	const auto b2 = Shadow<vector<int>>(v2);
	logical_assert(b2.size() == v2.size(), "Wrong size.");
	logical_assert(b2.for_any([](int el) { return el < 100; }), "Wrong result of parallel computation.");

	const auto v3 = random_int_vector(r++, 10000);
	const auto b3a = Shadow<vector<int>>(v3);
	logical_assert(b3a.size() == v3.size(), "Wrong size.");
	const auto b3b = b3a.sort([](int el) { return 0.1 * el + 1.0; });
	logical_assert(b3b.size() == v3.size(), "Wrong size.");
	for(size_t i = 0; i < b3b.size() - 1; i++)
		logical_assert(b3b[i] <= b3b[i + 1], "Sorting error.");
	
	const auto v4 = random_int_vector(r++, 10000);
	const auto b4a = Shadow<vector<int>>(v4);
	const auto b4b = b4a.sort_unique([](int el) { return 0.1 * el + 1.0; });
	auto v4s = unordered_set<int>();
	for(auto el : v4)
		v4s.insert(el);
	logical_assert(b4b.size() == v4s.size(), "Wrong size.");
	for(size_t i = 0; i < b4b.size() - 1; i++)
	{
		logical_assert(b4b[i] <= b4b[i + 1], "Sorting error.");
		logical_assert(b4b[i] != b4b[i + 1], "Uniqueness error.");
	}

	auto v5 = vector<int>();
	for(size_t i = 0; i < 1000; i++)
		v5.push_back(i);
	v5.push_back(1000000);
	
	const auto v5a = Shadow<vector<int>>(v5);
	logical_assert(v5a.for_all([](int el) { return el <= 1000000; }), "Wrong result of parallel computation.");

	const auto v5b = Shadow<vector<int>>(v5);
	logical_assert(v5b.for_any([](int el) { return el == 1000000; }), "Wrong result of parallel computation.");
	
	const auto v5c = Unfold<int>(v5);
	logical_assert(v5c.for_all([](int el) { return el <= 1000000; }), "Wrong result of parallel computation.");

	const auto v5d = Unfold<int>(v5);
	logical_assert(v5d.for_any([](int el) { return el == 1000000; }), "Wrong result of parallel computation.");
	
	const auto u0 = vector<int>({1});
	const auto u1 = Unfold<int>(u0);
	logical_assert(type_name<decltype(u1[0])>() == "int const&");
	const auto u2 = Shadow(u1);
	logical_assert(type_name<decltype(u2[0])>() == "int const&");
	const auto u3 = Unfold<int>(u2);
	logical_assert(type_name<decltype(u3[0])>() == "int const&");

	collections_address_test();
	collections_difference_test();
}


} // namespace Logical

#endif // DEBUG

#endif // LOGICAL_COLLECTIONS_HH
