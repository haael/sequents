



#ifndef LOGICAL_ERRORS_HH
#define LOGICAL_ERRORS_HH

#include <string>

#ifdef DEBUG
namespace execinfo
{
#include <execinfo.h>
};
#endif

namespace Logical
{

using std::string;

struct Error
{
#ifdef DEBUG
	static constexpr size_t stack_max = 256;
	void* stack[stack_max];
	size_t stack_size;
#endif

	const string message;

	Error(const char* msg)
	 : message(msg)
	{
#ifdef DEBUG
		stack_size = execinfo::backtrace(stack, stack_max);
#endif
	}
};


struct RuntimeError : public Error
{
	RuntimeError(const char* msg)
	 : Error(msg)
	{
	}
};


struct AssertionError : public Error
{
	int line;
	const char* file;

	AssertionError(const char* msg, int l, const char* f)
	 : Error(msg)
	 , line(l)
	 , file(f)
	{
	}
};

struct CollectionError : public Error
{
	CollectionError(const char* msg)
	 : Error(msg)
	{
	}
};

struct IndexError : public CollectionError
{
	size_t index;
	size_t size;

	IndexError(const char* msg, size_t i, size_t s)
	 : CollectionError(msg)
	 , index(i)
	 , size(s)
	{
	}
};

struct IteratorError : public CollectionError
{
	size_t index;
	void* collection1;
	void* collection2;

	IteratorError(const char* msg, size_t i, void* c1, void* c2)
	 : CollectionError(msg)
	 , index(i)
	 , collection1(c1)
	 , collection2(c2)
	{
	}
};

struct ConcurrencyError : public Error
{
	ConcurrencyError(const char* msg)
	 : Error(msg)
	{
	}
};

struct ThreadError : public ConcurrencyError
{
	ThreadError(const char* msg)
	 : ConcurrencyError(msg)
	{
	}
};

struct DeadlockError : public ConcurrencyError
{
	DeadlockError(const char* msg)
	 : ConcurrencyError(msg)
	{
	}
};

struct LockingError : public ConcurrencyError
{
	LockingError(const char* msg)
	 : ConcurrencyError(msg)
	{
	}
};

struct TransactionError : public Error
{
	TransactionError(const char* msg)
	 : Error(msg)
	{
	}
};

#ifdef DEBUG

#define assert_1(x) do_assert((x), (#x), __LINE__, __FILE__)
#define assert_2(x, y) do_assert((x), (y), __LINE__, __FILE__)

#define GET_3rd_ARG(arg1, arg2, arg3, ...) arg3
#define assert_chooser(...) GET_3rd_ARG(__VA_ARGS__, assert_2, assert_1)

#define logical_assert(...) assert_chooser(__VA_ARGS__)(__VA_ARGS__)


inline void do_assert(bool expr, const char* msg, int l, const char* f)
{
	if(!expr) throw AssertionError(msg, l, f);
}

#endif

} // namespace Logical

#endif // LOGICAL_ERRORS_HH
