
#ifndef LOGICAL_EXPRESSION_HH
#define LOGICAL_EXPRESSION_HH

#include "errors.hh"
#include "logical.hh"
#include "utils.hh"
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#ifdef DEBUG
#define inheritance_cast dynamic_cast
#else
#define inheritance_cast static_cast
#endif

namespace Logical
{

using std::bad_cast;
using std::cout;
using std::endl;
using std::make_shared;
using std::ostream;
using std::shared_ptr;
using std::string;
using std::type_info;
using std::unordered_set;
using std::vector;

class Expression;
class ExpressionReference;
class ExpressionIterator;
class ExpressionsIdentical;
class Variable;
class VariableHash;
class VariablesIdentical;

typedef unordered_set<Variable, VariableHash, VariablesIdentical> VariableSet;
typedef unordered_map<Variable, ExpressionReference, VariableHash, VariablesIdentical> Substitution;

struct VariableHash
{
	constexpr VariableHash(void)
	{
	}
	uint64_t operator()(const Variable&) const;
};

struct VariablesIdentical
{
	constexpr VariablesIdentical(void)
	{
	}
	bool operator()(const Variable&, const Variable&) const;
};

struct ExpressionsIdentical
{
	constexpr ExpressionsIdentical(void)
	{
	}
	bool operator()(const Expression&, const Expression&) const;
};

class Expression
{
public:
	enum class Type : uint8_t
	{
		EXPRESSION,
		REFERENCE,
		VARIABLE
	};

	virtual ExpressionReference substitute(const Substitution&) const = 0;
	virtual Type get_type(void) const
	{
		return Type::EXPRESSION;
	}
	virtual bool is_variable(void) const = 0;
	virtual bool is_ground(void) const = 0;
	virtual VariableSet free_variables(void) const = 0;
	virtual uint64_t hash(uint64_t seed = 0) const = 0;
	virtual bool identical(const Expression&) const = 0;

	virtual size_t size(void) const = 0;
	virtual size_t count(const Expression&) const = 0;
	virtual ExpressionReference operator[](size_t) const = 0;
	virtual ExpressionIterator begin(void) const;
	virtual ExpressionIterator end(void) const;
};

class ExpressionReference : public Expression
{
private:
	std::shared_ptr<const Expression> original;

public:
	template <typename PointerInitializer>
	ExpressionReference(PointerInitializer&& o)
	 : original(make_shared<typename remove_reference<PointerInitializer>::type>(o))
	{
	}

	virtual const Expression& get_expression(void) const
	{
		return *original;
	}

	virtual ExpressionReference substitute(const Substitution& substitution) const
	{
		return original->substitute(substitution);
	}
	virtual Type get_type(void) const
	{
		return Type::REFERENCE;
	}
	virtual bool is_variable(void) const
	{
		return original->is_variable();
	}
	virtual bool is_ground(void) const
	{
		return original->is_ground();
	}
	virtual VariableSet free_variables(void) const;
	virtual uint64_t hash(uint64_t seed = 0) const
	{
		return original->hash(seed);
	}

	virtual bool identical(const Expression& other) const
	{
		if(other.get_type() == Type::REFERENCE)
		{
			return original->identical(inheritance_cast<const ExpressionReference&>(other).get_expression());
		}
		else
		{
			return original->identical(other);
		}
	}

	virtual size_t size(void) const
	{
		return original->size();
	}
	virtual size_t count(const Expression& child) const
	{
		return original->count(child);
	}
	virtual ExpressionReference operator[](size_t index) const
	{
		return (*original)[index];
	}
	virtual ExpressionIterator begin(void) const;
	virtual ExpressionIterator end(void) const;
};

class Variable : public Expression
{
private:
	string name;

public:
	Variable(const string& si)
	 : name(si)
	{
	}
	Variable(string&& si)
	 : name(move(si))
	{
	}
	Variable(const Variable& v)
	 : name(v.name)
	{
	}
	Variable(Variable&& v)
	 : name(move(v.name))
	{
	}

	virtual const string& get_name(void) const
	{
		return name;
	}

	virtual ExpressionReference substitute(const Substitution& substitution) const;
	virtual Type get_type(void) const
	{
		return Type::VARIABLE;
	}
	virtual bool is_variable(void) const
	{
		return true;
	}
	virtual bool is_ground(void) const
	{
		return false;
	}
	virtual VariableSet free_variables(void) const;

	virtual uint64_t hash(uint64_t seed = 2937481) const
	{
		seed += 19;
		for(char c : name)
			seed = (323 * seed + (unsigned char)c + 29) ^ (seed >> (64 - 8));
		return seed;
	}

	virtual bool identical(const Expression& other) const
	{
		if(other.get_type() == Type::REFERENCE)
		{
			return identical(inheritance_cast<const ExpressionReference&>(other).get_expression());
		}
		else if(other.get_type() == Type::VARIABLE)
		{
			return name == inheritance_cast<const Variable&>(other).get_name();
		}
		else
		{
			return false;
		}
	}

	virtual size_t size(void) const
	{
		return 0;
	}
	virtual size_t count(const Expression&) const
	{
		return 0;
	}
	virtual ExpressionReference operator[](size_t index) const
	{
		throw ExpressionIndexError("Variable has no children.", index, size(), *this);
	}
};

class ExpressionIterator
{
private:
	const Expression& parent;
	size_t index;
	static constexpr auto identical = ExpressionsIdentical();

public:
	ExpressionIterator(const Expression& p, size_t i)
	 : parent(p)
	 , index(i)
	{
	}

	ExpressionReference operator*(void)const;
	ExpressionIterator& operator++(void)
	{
		index++;
		return *this;
	}
	ExpressionIterator& operator--(void)
	{
		index--;
		return *this;
	}
	ExpressionIterator& operator+=(intptr_t shift)
	{
		index += shift;
		return *this;
	}
	ExpressionIterator& operator-=(intptr_t shift)
	{
		index -= shift;
		return *this;
	}
	ExpressionIterator operator+(intptr_t shift) const
	{
		return ExpressionIterator(parent, index + shift);
	}
	ExpressionIterator operator-(intptr_t shift) const
	{
		return ExpressionIterator(parent, index - shift);
	}
	bool operator==(const ExpressionIterator& other) const
	{
		if(identical(parent, other.parent))
			return index == other.index;
		else
			throw ExpressionIteratorError("Expression ExpressionIterators are not comparable.", *this, other);
	}
	bool operator!=(const ExpressionIterator& other) const
	{
		if(identical(parent, other.parent))
			return index != other.index;
		else
			throw ExpressionIteratorError("Expression ExpressionIterators are not comparable.", *this, other);
	}
	bool operator<=(const ExpressionIterator& other) const
	{
		if(identical(parent, other.parent))
			return index <= other.index;
		else
			throw ExpressionIteratorError("Expression ExpressionIterators are not comparable.", *this, other);
	}
	bool operator>(const ExpressionIterator& other) const
	{
		if(identical(parent, other.parent))
			return index > other.index;
		else
			throw ExpressionIteratorError("Expression ExpressionIterators are not comparable.", *this, other);
	}
	bool operator>=(const ExpressionIterator& other) const
	{
		if(identical(parent, other.parent))
			return index >= other.index;
		else
			throw ExpressionIteratorError("Expression ExpressionIterators are not comparable.", *this, other);
	}
	bool operator<(const ExpressionIterator& other) const
	{
		if(identical(parent, other.parent))
			return index < other.index;
		else
			throw ExpressionIteratorError("Expression ExpressionIterators are not comparable.", *this, other);
	}
	operator bool(void) const
	{
		return 0 <= index && index < parent.size();
	}
};

inline uint64_t VariableHash::operator()(const Variable& e) const
{
	return e.hash();
}
inline bool ExpressionsIdentical::operator()(const Expression& a, const Expression& b) const
{
	return a.identical(b);
}
inline bool VariablesIdentical::operator()(const Variable& a, const Variable& b) const
{
	return a.identical(b);
}
inline VariableSet Variable::free_variables(void) const
{
	return VariableSet({*this});
}
inline ExpressionReference ExpressionIterator::operator*(void)const
{
	return parent[index];
}
inline VariableSet ExpressionReference::free_variables(void) const
{
	return original->free_variables();
}
inline ExpressionReference Variable::substitute(const Substitution& substitution) const
{
	if(substitution.count(*this))
		return substitution.at(*this);
	else
		return ExpressionReference(*this);
}
inline ExpressionIterator Expression::begin(void) const
{
	return ExpressionIterator(*this, 0);
}
inline ExpressionIterator Expression::end(void) const
{
	return ExpressionIterator(*this, size());
}
inline ExpressionIterator ExpressionReference::begin(void) const
{
	return original->begin();
}
inline ExpressionIterator ExpressionReference::end(void) const
{
	return original->end();
}

} // namespace Logical

#ifdef DEBUG

namespace Logical
{

void expression_test(void)
{
	const auto identical = ExpressionsIdentical();

	const auto a = Variable("a");
	const auto b = Variable("b");
	const auto ra = ExpressionReference(a);
	const auto rb = ExpressionReference(b);
	const auto rra = ExpressionReference(ra);
	const auto rrb = ExpressionReference(rb);

	logical_assert(identical(a, a));
	logical_assert(identical(b, b));
	logical_assert(!identical(a, b));
	logical_assert(!identical(b, a));

	logical_assert(identical(a, ra));
	logical_assert(identical(b, rb));
	logical_assert(!identical(a, rb));
	logical_assert(!identical(b, ra));

	logical_assert(identical(ra, a));
	logical_assert(identical(rb, b));
	logical_assert(!identical(ra, b));
	logical_assert(!identical(rb, a));

	logical_assert(identical(ra, ra));
	logical_assert(identical(rb, rb));
	logical_assert(!identical(ra, rb));
	logical_assert(!identical(rb, ra));

	logical_assert(identical(a, rra));
	logical_assert(identical(b, rrb));
	logical_assert(!identical(a, rrb));
	logical_assert(!identical(b, rra));

	logical_assert(identical(rra, ra));
	logical_assert(identical(rrb, rb));
	logical_assert(!identical(rra, rb));
	logical_assert(!identical(rrb, ra));

	logical_assert(identical(rra, ra));
	logical_assert(identical(rrb, rb));
	logical_assert(!identical(rra, rb));
	logical_assert(!identical(rrb, ra));

	logical_assert(identical(rra, rra));
	logical_assert(identical(rrb, rrb));
	logical_assert(!identical(rra, rrb));
	logical_assert(!identical(rrb, rra));
}

} // namespace Logical

#endif // DEBUG

#endif // LOGICAL_EXPRESSION_HH
