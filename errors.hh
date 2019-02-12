
#ifndef LOGICAL_ERRORS_HH
#define LOGICAL_ERRORS_HH

#include "logical.hh"
#include <memory>
#include <string>

#ifdef DEBUG
namespace execinfo
{
#include <execinfo.h>
};
#endif

namespace Logical
{

using std::make_unique;
using std::remove_reference;
using std::string;
using std::unique_ptr;

struct Error
{
#ifdef DEBUG
	static constexpr size_t stack_max = 256;
	void* stack[stack_max];
	size_t stack_size;
#endif

	const string message;

	Error(const string& msg)
	 : message(msg)
	{
#ifdef DEBUG
		stack_size = execinfo::backtrace(stack, stack_max);
#endif
	}
};


struct RuntimeError : public Error
{
	RuntimeError(const string& msg)
	 : Error(msg)
	{
	}
};


#ifdef DEBUG
struct AssertionError : public Error
{
	int line;
	const string& file;

	AssertionError(const string& msg, int l, const string& f)
	 : Error(msg)
	 , line(l)
	 , file(f)
	{
	}
};
#endif


struct CollectionError : public Error
{
	CollectionError(const string& msg)
	 : Error(msg)
	{
	}
};


struct GeneralIndexError : public CollectionError
{
	size_t index;
	size_t size;

	GeneralIndexError(const string& msg, size_t i, size_t s)
	 : CollectionError(msg)
	 , index(i)
	 , size(s)
	{
	}
};


template <typename Collection>
struct IndexError : public GeneralIndexError
{
	unique_ptr<Collection> collection;

	IndexError(const string& msg, size_t i, size_t s, const Collection& c)
	 : GeneralIndexError(msg, i, s)
	 , collection(make_unique<Collection>(c))
	{
	}
};


template <typename Collection1, typename Collection2>
struct IteratorError : public CollectionError
{
	size_t index;
	unique_ptr<Collection1> collection1;
	unique_ptr<Collection2> collection2;

	IteratorError(const string& msg, size_t i, const Collection1& c1, const Collection2& c2)
	 : CollectionError(msg)
	 , index(i)
	 , collection1(make_unique<Collection1>(c1))
	 , collection2(make_unique<Collection1>(c2))
	{
	}
};


struct ConcurrencyError : public Error
{
	ConcurrencyError(const string& msg)
	 : Error(msg)
	{
	}
};


struct ThreadError : public ConcurrencyError
{
	ThreadError(const string& msg)
	 : ConcurrencyError(msg)
	{
	}
};


struct DeadlockError : public ConcurrencyError
{
	DeadlockError(const string& msg)
	 : ConcurrencyError(msg)
	{
	}
};


struct LockingError : public ConcurrencyError
{
	LockingError(const string& msg)
	 : ConcurrencyError(msg)
	{
	}
};


struct TransactionError : public Error
{
	TransactionError(const string& msg)
	 : Error(msg)
	{
	}
};


struct ExpressionError : public Error
{
	ExpressionError(const string& msg)
	 : Error(msg)
	{
	}
};


struct GeneralExpressionIndexError : public ExpressionError
{
	size_t index;
	size_t size;

	GeneralExpressionIndexError(const string& msg, size_t i, size_t s)
	 : ExpressionError(msg)
	 , index(i)
	 , size(s)
	{
	}
};


template <typename ExpressionT>
struct ExpressionIndexError : public GeneralExpressionIndexError
{
	unique_ptr<ExpressionT> expression;

	ExpressionIndexError(const string& msg, size_t i, size_t s, const ExpressionT& e)
	 : GeneralExpressionIndexError(msg, i, s)
	 , expression(make_unique<ExpressionT>(e))
	{
	}
};


struct ExpressionIteratorError : public ExpressionError
{
	unique_ptr<ExpressionIterator> expression1;
	unique_ptr<ExpressionIterator> expression2;

	ExpressionIteratorError(const string& msg, const ExpressionIterator& e1, const ExpressionIterator& e2)
	 : ExpressionError(msg)
	 , expression1(make_unique<ExpressionIterator>(e1))
	 , expression2(make_unique<ExpressionIterator>(e2))
	{
	}
};

struct FormulaIndexError : public Error
{
	size_t index;
	size_t size;
	unique_ptr<Formula> formula;

	FormulaIndexError(const string& msg, size_t i, size_t s, const Formula& f)
	 : Error(msg)
	 , index(i)
	 , size(s)
	 , formula(make_unique<Formula>(f))
	{
	}
};


struct SequentError : public Error
{
	SequentError(const string& msg)
	 : Error(msg)
	{
	}
};


struct UnsupportedConnectiveError : public SequentError
{
	const Symbol& symbol;
	UnsupportedConnectiveError(const string& msg, const Symbol& s)
	 : SequentError(msg)
	 , symbol(s)
	{
	}
};


struct NullPointerError : public Error
{
	NullPointerError(const string& msg)
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

inline void do_assert(bool expr, const string& msg, int l, const string& f)
{
	if(!expr)
		throw AssertionError(msg, l, f);
}

#endif

} // namespace Logical

#endif // LOGICAL_ERRORS_HH
