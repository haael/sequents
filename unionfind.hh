

#ifndef LOGICAL_UNIONFIND_HH
#define LOGICAL_UNIONFIND_HH


#include <type_traits>
#include <unordered_map>


#include "errors.hh"
#include "logical.hh"
#include "sync.hh"


namespace Logical {


using std::unordered_map;
using std::result_of;
using std::is_same;
using std::declval;


template <typename Value>
class CompareCache
{
private:
	typedef uint64_t hash_type;
	typedef shared_mutex SharedMutex;
	typedef unordered_map<const Value*, hash_type> HashTable;
	typedef Transaction<HashTable, SharedMutex> HashTableTransaction;
	typedef unordered_map<const Value*, const Value*> ItemsTable;
	typedef Transaction<ItemsTable, SharedMutex> ItemsTableTransaction;
	
	const size_t max_hash_failures = 2;
	const size_t max_join_failures = 4;
	const size_t max_find_failures = 4;
	const size_t max_unlocked_equal_failures = 6;
	const size_t max_locked_equal_failures = 10;
	
	HashTable hashes;
	SharedMutex hashes_mutex;
	ItemsTable unionfind;
	SharedMutex unionfind_mutex;
	SharedMutex equal_mutex;

protected:
	static hash_type value_hash(const Value& value)
	{
		return value.hash();
	}
	
	static bool value_compare(const Value& one, const Value& two)
	{
		return one == two;
	}

private:
	hash_type hash(const Value& value)
	{
		ReadLockable hashes_mutex_rl(hashes_mutex);
		uint64_t result;
		size_t failures = 0;
		
		while(true)
		{
			try
			{
				HashTableTransaction store(hashes, hashes_mutex_rl);
				
				if(store.count(&value))
					result = store[&value];
				else
					result = store[&value] = value_hash(value);
				
				store.commit_transaction([result, &value](auto& store)->bool
				{
					return result == store[&value];
				});
				
				break;
			}
			catch(const TransactionError& htte)
			{
				if(++failures >= max_hash_failures)
					throw htte;
			}
		}
		
		return result;
	}

	void join(const Value& one, const Value& two)
	{
		ReadLockable unionfind_mutex_rl(unionfind_mutex);
		size_t failures = 0;
		
		while(true)
		{
			try
			{
				ItemsTableTransaction store(unionfind, unionfind_mutex_rl);

				const Value* p_one;
				if(store.count(&one))
					p_one = store[&one];
				else
					p_one = store[&one] = &one;

				const Value* p_two;
				if(store.count(&two))
					p_two = store[&two];
				else
					p_two = store[&two] = &two;

				if(p_one > p_two)
					store[&one] = p_two;
				else if(p_two > p_one)
					store[&two] = p_one;

				store.commit_transaction([&one, &two](auto& store) -> bool
				{
					return store[&one] == store[&two];
				});
				
				break;
			}
			catch(const TransactionError& htte)
			{
				if(++failures >= max_join_failures)
					throw htte;
			}
		}
	}
	
	bool find(const Value& one, const Value& two)
	{
		ReadLockable unionfind_mutex_rl(unionfind_mutex);
		size_t failures = 0;
		bool result;
		
		while(true)
		{
			try
			{
				ItemsTableTransaction store(unionfind, unionfind_mutex_rl);

				const Value* p_one = &one;
				while(store.count(p_one) && store[p_one] != p_one)
					p_one = store[p_one];
				store[&one] = p_one;

				const Value* p_two = &two;
				while(store.count(p_two) && store[p_two] != p_two)
					p_two = store[p_two];
				store[&two] = p_two;
				
				result = (p_one == p_two);
				
				store.commit_transaction([&one, &two, p_one, p_two](auto& store) -> bool
				{
					return (store[&one] == p_one) && (store[&two] == p_two);
				});
				
				break;
			}
			catch(const TransactionError& htte)
			{
				if(++failures > max_find_failures)
					throw htte;
			}
		}
		
		return result;
	}

public:
	bool equal(const Value& one, const Value& two)
	{
		ReadLockable equal_mutex_rl(equal_mutex);
		size_t failures = 0;

		while(true)
		{
			try
			{
				auto lock = SharedLock<SharedMutex>(equal_mutex_rl);
				if(failures >= max_unlocked_equal_failures) lock.upgrade();

				if(&one == &two) return true;

				if(find(one, two)) return true;

				//if(partition(one, two)) return false;

				if(hash(one) != hash(two))
				{
					//refine(one, two);
					return false;
				}
				else if(value_compare(one, two))
				{
					join(one, two);
					return true;
				}
				else
				{
					//refine(one, two);
					return false;
				}
			}
			catch(const TransactionError& te)
			{
				if(++failures > max_locked_equal_failures)
					throw te;
			}
		}
	}
};

template<>
CompareCache<uintptr_t>::hash_type CompareCache<uintptr_t>::value_hash(const uintptr_t& value)
{
	return value;
}


} // namespace Logical


#ifdef DEBUG

namespace Logical {


static inline void unionfind_test(void)
{
	CompareCache<uintptr_t> compare_cache;
	
	static const uintptr_t a = 1;
	static const uintptr_t b = 1;
	static const uintptr_t c = 2;
	
	logical_assert(compare_cache.equal(a, a), "(round 1) a = 1 should equal a = 1");
	logical_assert(compare_cache.equal(a, b), "(round 1) a = 1 should equal b = 1");
	logical_assert(!compare_cache.equal(a, c), "(round 1) a = 1 shouldn't equal c = 2");

	logical_assert(compare_cache.equal(b, a), "(round 1) b = 1 should equal a = 1");
	logical_assert(compare_cache.equal(b, b), "(round 1) b = 1 should equal b = 1");
	logical_assert(!compare_cache.equal(b, c), "(round 1) b = 1 shouldn't equal c = 2");

	logical_assert(!compare_cache.equal(c, a), "(round 1) c = 2 shouldn't equal a = 1");
	logical_assert(!compare_cache.equal(c, b), "(round 1) c = 2 shouldn't equal b = 1");
	logical_assert(compare_cache.equal(c, c), "(round 1) c = 2 should equal c = 2");

	logical_assert(compare_cache.equal(a, a), "(round 2) a = 1 should equal a = 1");
	logical_assert(compare_cache.equal(a, b), "(round 2) a = 1 should equal b = 1");
	logical_assert(!compare_cache.equal(a, c), "(round 2) a = 1 shouldn't equal c = 2");

	logical_assert(compare_cache.equal(b, a), "(round 2) b = 1 should equal a = 1");
	logical_assert(compare_cache.equal(b, b), "(round 2) b = 1 should equal b = 1");
	logical_assert(!compare_cache.equal(b, c), "(round 2) b = 1 shouldn't equal c = 2");

	logical_assert(!compare_cache.equal(c, a), "(round 2) c = 2 shouldn't equal a = 1");
	logical_assert(!compare_cache.equal(c, b), "(round 2) c = 2 shouldn't equal b = 1");
	logical_assert(compare_cache.equal(c, c), "(round 2) c = 2 should equal c = 2");
}


} // namespace Logical


#endif // DEBUG


#endif // LOGICAL_UNIONFIND_HH
