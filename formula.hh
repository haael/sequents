
#ifndef LOGICAL_FORMULA_HH
#define LOGICAL_FORMULA_HH

#include "errors.hh"
#include "expression.hh"
#include "logical.hh"
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace Logical
{

using std::cout;
using std::endl;
using std::forward;
using std::hex;
using std::move;
using std::ostream;
using std::string_view;
using std::vector;
using std::optional;
using std::addressof;


class Symbol;
class Formula;


class Symbol
{
private:
	string_view value;
	bool rel, quant;

protected:
	constexpr Symbol(const string_view& s, bool r, bool q)
	 : value(s)
	 , rel(r)
	 , quant(q)
	{
	}

public:
	template <typename... Args>
	Formula operator()(Args&&... args) const;

	void operator=(const Symbol&) = delete;

	void print(ostream& out) const;

	const string_view& get_value(void) const;

	constexpr uint64_t hash(uint64_t seed = 0) const;

	constexpr operator uint64_t(void) const;

	bool operator==(const Symbol& that) const
	{
		return this == &that || (rel == that.rel && quant == that.quant && value == that.value);
	}

	bool operator!=(const Symbol& that) const
	{
		return !((*this) == that);
	}

	bool is_relation(void) const
	{
		return rel;
	}

	bool is_quantifier(void) const
	{
		return quant;
	}
};


class ConnectiveSymbol : public Symbol
{
public:
	constexpr ConnectiveSymbol(const string_view& s)
	 : Symbol(s, false, false)
	{
	}

	template <typename... Args>
	Formula operator()(Args&&... args) const;
};


class QuantifierSymbol : public Symbol
{
public:
	constexpr QuantifierSymbol(const string_view& s)
	 : Symbol(s, false, true)
	{
	}

	template <typename VariableT>
	class QuantifierApplication
	{
	private:
		VariableT variable;
		const Symbol& symbol;

	public:
		QuantifierApplication(VariableT&& v, const Symbol& s)
		 : variable(forward<VariableT>(v))
		 , symbol(s)
		{
		}

		template <typename... Args>
		Formula operator()(Args&&... args);
	};

	template <typename VariableT>
	auto operator[](VariableT&& var) const
	{
		return QuantifierApplication<VariableT>(forward<VariableT>(var), *this);
	}
};


class RelationSymbol : public Symbol
{
public:
	constexpr RelationSymbol(const string_view& s)
	 : Symbol(s, true, false)
	{
	}

	template <typename... Args>
	Formula operator()(Args&&... args) const;
};

struct SymbolHash
{
	constexpr SymbolHash(void)
	{
	}

	uint64_t operator()(const Symbol& s) const
	{
		return s.hash();
	}
};


class Formula
{
#ifdef DEBUG
public:
	static mutex active_objects_mutex;
	static unordered_set<const Formula*> active_objects;
	
	static bool is_valid_object(const Formula* f)
	{
		return active_objects.count(f);
	}
#endif

private:
	const Symbol& symbol;
	
	union
	{
		vector<Formula> formula;
		vector<ExpressionReference> expression;
	};
	unique_ptr<const Variable> variable;

public:
	class FormulaOrExpression
	{
	private:
		union
		{
			const Formula* formula;
			const Expression* expression;
		};
		bool relation;

	public:
		typedef FormulaOrExpression value_type;

		FormulaOrExpression(const Formula& f)
		 : formula(&f)
		 , relation(false)
		{
		}

		FormulaOrExpression(const Expression& e)
		 : expression(&e)
		 , relation(true)
		{
		}

		FormulaOrExpression(const FormulaOrExpression& fe)
		 : relation(fe.relation)
		{
			if(relation)
				new(&expression) auto(fe.expression);
			else
				new(&formula) auto(fe.formula);
		}

		FormulaOrExpression(FormulaOrExpression&& fe)
		 : relation(move(fe.relation))
		{
			if(relation)
				new(&expression) auto(move(fe.expression));
			else
				new(&formula) auto(move(fe.formula));
		}

		operator const Formula&(void)const
		{
			if(relation)
				throw RuntimeError("Requesting formula field when the symbol is a relation.");
			else
				return *formula;
		}

		operator const Expression&(void)const
		{
			if(relation)
				return *expression;
			else
				throw RuntimeError("Requesting expression field when the symbol is not a relation.");
		}

		const void* operator&(void)const
		{
			if(relation)
				return expression;
			else
				return formula;
		}

		FormulaOrExpression operator[](size_t index)
		{
			if(relation)
				return expression[index];
			else
				return formula[index];
		}
	};

	typedef FormulaOrExpression value_type;

	Formula(const Formula& f)
	 : symbol(f.symbol)
	{
		if(f.variable)
			throw RuntimeError("Not implemented yet."); // TODO

		if(symbol.is_relation())
			new(&expression) auto(f.expression);
		else
			new(&formula) auto(f.formula);

#ifdef DEBUG
		{
			lock_guard<mutex> lg(active_objects_mutex);
			//cerr << " Formula copy " << this << " from " << (const Formula*)(&f) << endl;
			active_objects.insert(this);
		}
#endif
	}

	Formula(Formula&& f)
	 : symbol(move(f.symbol))
	 , variable(move(f.variable))
	{
		if(symbol.is_relation())
			new(&expression) auto(move(f.expression));
		else
			new(&formula) auto(move(f.formula));

#ifdef DEBUG
		{
			lock_guard<mutex> lg(active_objects_mutex);
			//cerr << " Formula move " << this << " from " << (const Formula*)(&f) << endl;
			active_objects.insert(this);
		}
#endif
	}

	template <typename FormulaVector, typename VariableT>
	Formula(const Symbol& s, FormulaVector&& f, VariableT&& v)
	 : symbol(s)
	 , formula(forward<FormulaVector>(f))
	 , variable(make_unique<typename remove_reference<VariableT>::type>(forward<VariableT>(v)))
	{
		logical_assert(!s.is_relation());
		logical_assert(s.is_quantifier());
#ifdef DEBUG
		{
			lock_guard<mutex> lg(active_objects_mutex);
			//cerr << " Formula create (quantifier) " << this << endl;
			active_objects.insert(this);
		}
#endif
	}

	Formula(const Symbol& s, const vector<Formula>& f)
	 : symbol(s)
	 , formula(f)
	{
		logical_assert(!s.is_relation());
		logical_assert(!s.is_quantifier());
#ifdef DEBUG
		{
			lock_guard<mutex> lg(active_objects_mutex);
			//cerr << " Formula create (connective, copy vector) " << this << endl;
			active_objects.insert(this);
		}
#endif
	}

	Formula(const Symbol& s, vector<Formula>&& f)
	 : symbol(s)
	 , formula(move(f))
	{
		logical_assert(!s.is_relation());
		logical_assert(!s.is_quantifier());
#ifdef DEBUG
		{
			lock_guard<mutex> lg(active_objects_mutex);
			//cerr << " Formula create (connective, move vector) " << this << endl;
			active_objects.insert(this);
		}
#endif
	}

	Formula(const Symbol& s, const vector<ExpressionReference>& e)
	 : symbol(s)
	 , expression(e)
	{
		logical_assert(s.is_relation());
#ifdef DEBUG
		{
			lock_guard<mutex> lg(active_objects_mutex);
			//cerr << " Formula create (relation, copy vector) " << this << endl;
			active_objects.insert(this);
		}
#endif
	}

	Formula(const Symbol& s, vector<ExpressionReference>&& e)
	 : symbol(s)
	 , expression(move(e))
	{
		logical_assert(s.is_relation());
#ifdef DEBUG
		{
			lock_guard<mutex> lg(active_objects_mutex);
			//cerr << " Formula create (relation, move vector) " << this << endl;
			active_objects.insert(this);
		}
#endif
	}

	bool is_ground(void) const
	{
		if(variable)
		{
			if(symbol.is_relation())
			{
				for(const auto& e : expression)
					if(!e.is_ground())
					{
						const auto evf = e.free_variables();
						if(evf.size() - evf.count(*variable))
							return false;
					}
			}
			else
			{
				for(const auto& f : formula)
					if(!f.is_ground())
					{
						const auto fvf = f.free_variables();
						if(fvf.size() - fvf.count(*variable))
							return false;
					}
			}
		}
		else
		{
			if(symbol.is_relation())
			{
				for(const auto& e : expression)
					if(!e.is_ground())
						return false;
			}
			else
			{
				for(const auto& f : formula)
					if(!f.is_ground())
						return false;
			}
		}

		return true;
	}

	VariableSet free_variables(void) const
	{
		VariableSet vars;

		if(symbol.is_relation())
		{
			for(const auto& e : expression)
				vars.merge(e.free_variables());
		}
		else
		{
			for(const auto& f : formula)
				vars.merge(f.free_variables());
		}

		return vars;
	}

	void print(ostream& out) const;

	uint64_t hash(uint64_t seed = 0) const
	{
		seed ^= symbol.hash(seed);
		if(symbol.is_relation())
		{
			for(const auto& e : expression)
				seed ^= e.hash(seed + 3);
		}
		else
		{
			for(const auto& f : formula)
				seed ^= f.hash(seed);
		}
		return seed;
	}

	bool operator==(const Formula& that) const
	{
		static const auto expressions_identical = ExpressionsIdentical();

#if defined(DEBUG) && !defined(__clang__)
		if(!this)
			throw RuntimeError("'this' pointer is null");
		if(!&that)
			throw RuntimeError("'&that' pointer is null");
#endif

		if(this == &that)
			return true;

		if(symbol != that.symbol)
			return false;

		if(symbol.is_relation())
		{
			if(expression.size() != that.expression.size())
				return false;

			for(size_t i = 0; i < expression.size(); i++)
				if(!expressions_identical(expression[i], that.expression[i]))
					return false;
			return true;
		}
		else
			return formula == that.formula;
	}

	bool operator!=(const Formula& that) const
	{
		return !((*this) == that);
	}

	const Symbol& get_symbol(void) const
	{
		return symbol;
	}

	bool has_symbol(const Symbol& s) const
	{
		return s == symbol;
	}

	template <typename Symbols>
	bool has_symbols(const Symbols& ss)
	{
		for(const Symbol& s : ss)
			if(s == symbol)
				return true;
		return false;
	}

	size_t size(void) const
	{
		if(symbol.is_relation())
			return expression.size();
		else
			return formula.size();
	}

	FormulaOrExpression operator[](const size_t index) const
	{
		if(symbol.is_relation())
		{
			if(index >= expression.size())
				throw FormulaIndexError("Sub-expression index out of range.", index, expression.size(), *this);
			return expression[index];
		}
		else
		{
			if(index >= formula.size())
				throw FormulaIndexError("Sub-formula index out of range.", index, formula.size(), *this);
			return formula[index];
		}
	}

	auto begin(void) const
	{
		if(symbol.is_relation())
			throw RuntimeError("Iterating over expression not implemented yet.");
		return formula.begin();
	} // TODO

	auto end(void) const
	{
		if(symbol.is_relation())
			throw RuntimeError("Iterating over expression not implemented yet.");
		return formula.end();
	} // TODO

	size_t total_size(void) const;

	size_t depth(void) const
	{
		if(symbol.is_relation())
			return 1;

		size_t d = 0;
		for(const auto& f : formula)
		{
			const size_t nd = f.depth();
			if(nd > d)
				d = nd;
		}
		return d + 1;
	}

	// template<FormulaRF> Formula operator / (FormulaRF&& d) const;
	template <typename FormulaRF>
	Formula operator%(FormulaRF&&) const;

	template <typename FormulaRF>
	Formula operator<<(FormulaRF&&) const;

	template <typename FormulaRF>
	Formula operator>>(FormulaRF&&) const;
	
	Formula operator~(void) const;
	
	template <typename FormulaRF>
	Formula operator&(FormulaRF&&) &&;

	template <typename FormulaRF>
	Formula operator|(FormulaRF&&) const;

	template <typename FormulaRF>
	Formula operator^(FormulaRF&&) const;

	// virtual Formula substitute(const unordered_map<Expression, Expression>&) const;

#ifdef DEBUG
	class TracingPointer
	{
	private:
		const Formula* target;
	
	public:
		TracingPointer(const Formula& t)
		 : target(addressof(t))
		{
			if(target)
			{
				lock_guard<mutex> lg(target->tracing_pointers_mutex);
				target->tracing_pointers.insert(this);
			}
		}
		
		TracingPointer(nullptr_t zero)
		 : target(nullptr)
		{
		}
		
		TracingPointer(const TracingPointer& cp)
		 : target(cp.target)
		{
			if(target)
			{
				lock_guard<mutex> lg(target->tracing_pointers_mutex);
				target->tracing_pointers.insert(this);
			}
		}
		
		TracingPointer(TracingPointer&& mv)
		 : target(move(mv.target))
		{
			if(target)
			{
				lock_guard<mutex> lg(target->tracing_pointers_mutex);
				target->tracing_pointers.insert(this);
				target->tracing_pointers.erase(&mv);
			}
		}
				
		operator const Formula* (void) const
		{
			return target;
		}
		
		const Formula& operator * (void) const
		{
			if(!target) throw NullPointerError("Dereferencing null TracingPointer.");
			return *target;
		}
		
		const Formula* operator -> (void) const
		{
			if(!target) throw NullPointerError("Accessing member of null TracingPointer.");
			return target;
		}
		
		operator bool (void) const
		{
			return target;
		}
		
		bool operator == (const TracingPointer& other) const
		{
			return target == other.target;
		}
		
		bool operator != (const TracingPointer& other) const
		{
			return target != other.target;
		}
		
		bool operator <= (const TracingPointer& other) const
		{
			return target <= other.target;
		}
		
		bool operator > (const TracingPointer& other) const
		{
			return target > other.target;
		}
		
		bool operator >= (const TracingPointer& other) const
		{
			return target >= other.target;
		}
		
		bool operator < (const TracingPointer& other) const
		{
			return target < other.target;
		}
		
		TracingPointer& operator = (const TracingPointer& other)
		{
			if(target)
			{
				lock_guard<mutex> lg(target->tracing_pointers_mutex);
				target->tracing_pointers.erase(this);
			}
			
			target = other.target;
			
			if(target)
			{
				lock_guard<mutex> lg(target->tracing_pointers_mutex);
				target->tracing_pointers.insert(this);
			}
			
			return *this;
		}
		
		TracingPointer& operator = (TracingPointer&& other)
		{
			if(target)
			{
				lock_guard<mutex> lg(target->tracing_pointers_mutex);
				target->tracing_pointers.erase(this);
			}
			
			target = other.target;
			
			if(target)
			{
				lock_guard<mutex> lg(target->tracing_pointers_mutex);
				target->tracing_pointers.insert(this);
				target->tracing_pointers.erase(&other);
				other.target = nullptr;
			}
			
			return *this;
		}
		
		TracingPointer& operator = (nullptr_t zero)
		{
			if(target)
			{
				lock_guard<mutex> lg(target->tracing_pointers_mutex);
				target->tracing_pointers.erase(this);
			}
			
			target = nullptr;
			
			return *this;
		}
		
		~TracingPointer(void)
		{
			if(target)
			{
				lock_guard<mutex> lg(target->tracing_pointers_mutex);
				target->tracing_pointers.erase(this);
			}
		}
	};
	
	mutable mutex tracing_pointers_mutex;
	mutable unordered_set<const TracingPointer*> tracing_pointers;
	
	TracingPointer operator& (void) const
	{
		return TracingPointer(*this);
	}
#endif

	~Formula(void)
	{
#ifdef DEBUG
		{
			lock_guard<mutex> lg(active_objects_mutex);
			if(active_objects.count(this) != 1)
			{
				//cerr << " ~Formula error " << this << endl;
				abort();
			}
			else
				;//cerr << " ~Formula destroy " << this << endl;
			active_objects.erase(this);
		}
		
		{
			lock_guard<mutex> lg(tracing_pointers_mutex);
			logical_assert(tracing_pointers.empty());
		}
#endif

		if(symbol.is_relation())
			expression.~vector<ExpressionReference>();
		else
			formula.~vector<Formula>();
	}
};


class CompoundFormula : public Formula
{
public:
	typedef Formula value_type;

	const Formula& operator[](const size_t index) const
	{
		return Formula::operator[](index);
	}
};


class AtomicFormula : public Formula
{
public:
	typedef Expression value_type;

	const Expression& operator[](const size_t index) const
	{
		return Formula::operator[](index);
	}
};


inline ostream& operator<<(ostream& stream, const Formula& f)
{
	f.print(stream);
	return stream;
}

inline ostream& operator<<(ostream& stream, const Symbol& s)
{
	s.print(stream);
	return stream;
}

template <typename... Args>
inline Formula ConnectiveSymbol::operator()(Args&&... args) const
{
	return Formula(*this, vector<Formula>({forward<Args>(args)...}));
}

template <typename VariableT>
template <typename... Args>
inline Formula QuantifierSymbol::QuantifierApplication<VariableT>::operator()(Args&&... args)
{
	return Formula(symbol, vector<Formula>({forward<Args>(args)...}), forward<VariableT>(variable));
}

template <typename... Args>
inline Formula RelationSymbol::operator()(Args&&... args) const
{
	return Formula(*this, vector<ExpressionReference>({forward<Args>(args)...}));
}

inline constexpr uint64_t Symbol::hash(uint64_t seed) const
{
	seed += rel * 109 + quant * 113 + 37;
	for(char c : value)
		seed = (257 * seed + (unsigned char)c + 13) ^ (seed >> (64 - 8));
	return seed;
}

inline constexpr Symbol::operator uint64_t(void) const
{
	return hash(0x38a10a1c);
}

inline void Symbol::print(ostream& out) const
{
	out << value;
}

inline void Formula::print(ostream& out) const
{
#ifdef DEBUG
	logical_assert(is_valid_object(this));
#endif
	
	out << symbol;
	out << "(";
	bool first = true;
	for(auto& f : formula) // FIXME
	{
		if(first)
			first = false;
		else
			out << ",";
		out << f;
	}
	out << ")";
}

inline size_t Formula::total_size(void) const
{
#ifdef DEBUG
	{
		lock_guard<mutex> lg(active_objects_mutex);
		logical_assert(active_objects.count(this) == 1);
	}
/*{
	static mutex total_size_mutex;
	lock_guard<mutex> lg(total_size_mutex);
	cerr << hex << "total_size this=" << this << " symbol=" << &symbol << "/" << symbol << endl;
}*/
#endif

	size_t s = 1;
	if(symbol.is_relation())
	{
		return expression.size();
	}
	else
	{
		for(const auto& f : formula)
			s += f.total_size();
	}
	return s;
}

constexpr auto Id = ConnectiveSymbol("");
constexpr auto Not = ConnectiveSymbol("~");

constexpr auto And = ConnectiveSymbol("∧");
constexpr auto Or = ConnectiveSymbol("∨");
constexpr auto NAnd = ConnectiveSymbol("⊼");
constexpr auto NOr = ConnectiveSymbol("⊽");

constexpr auto Xor = ConnectiveSymbol("⊻");
constexpr auto NXor = ConnectiveSymbol("⩝");
constexpr auto Equiv = ConnectiveSymbol("↔");
constexpr auto NEquiv = ConnectiveSymbol("↮");

constexpr auto Impl = ConnectiveSymbol("→");
constexpr auto NImpl = ConnectiveSymbol("↛");
constexpr auto RImpl = ConnectiveSymbol("←");
constexpr auto NRImpl = ConnectiveSymbol("↚");

constexpr auto ForAll = QuantifierSymbol("∀");
constexpr auto Exists = QuantifierSymbol("∃");
// constexpr auto Unique = QuantifierSymbol("∃!");

constexpr auto True = ConnectiveSymbol("⊤");
constexpr auto False = ConnectiveSymbol("⊥");

constexpr auto Ident = RelationSymbol("≡");
constexpr auto NIdent = RelationSymbol("≢");
constexpr auto Equal = RelationSymbol("=");
constexpr auto NEqual = RelationSymbol("≠");

constexpr auto Pred = RelationSymbol("≺");
constexpr auto Succ = RelationSymbol("≻");
constexpr auto EPred = RelationSymbol("≼");
constexpr auto ESucc = RelationSymbol("≽");
constexpr auto NPred = RelationSymbol("⊀");
constexpr auto NSucc = RelationSymbol("⊁");

template <typename FormulaRF>
inline Formula Formula::operator%(FormulaRF&& that) const
{
	return Equiv(*this, forward<FormulaRF>(that));
}

template <typename FormulaRF>
inline Formula Formula::operator<<(FormulaRF&& that) const
{
	return Impl(*this, forward<FormulaRF>(that));
}

template <typename FormulaRF>
inline Formula Formula::operator>>(FormulaRF&& that) const
{
	return RImpl(*this, forward<FormulaRF>(that));
}

inline Formula Formula::operator~(void) const
{
	return Not(*this);
}

template <typename FormulaRF>
inline Formula Formula::operator&(FormulaRF&& that) &&
{
	return And(move(*this), forward<FormulaRF>(that));
}

template <typename FormulaRF>
inline Formula Formula::operator|(FormulaRF&& that) const
{
	return Or(*this, forward<FormulaRF>(that));
}

template <typename FormulaRF>
inline Formula Formula::operator^(FormulaRF&& that) const
{
	return Xor(*this, forward<FormulaRF>(that));
}

/*

constexpr auto Everyone = ConnectiveSymbol("◻");
constexpr auto Someone = ConnectiveSymbol("◇");

constexpr auto WillAlways = ConnectiveSymbol("⟥");
constexpr auto WillOnce = ConnectiveSymbol("⟣");
constexpr auto WasAlways = ConnectiveSymbol("⟤");
constexpr auto WasOnce = ConnectiveSymbol("⟢");



constexpr auto Id = Symbol("⍳");

constexpr auto Assert = Symbol("⇶");
constexpr auto Dissert = Symbol("↯");

constexpr auto Lesser = Symbol("<");
constexpr auto NLesser = Symbol("≥");
constexpr auto Greater = Symbol(">");
constexpr auto NGreater = Symbol("≤");

constexpr auto Member = Symbol("∈");
constexpr auto NMember = Symbol("∉");
constexpr auto RMember = Symbol("∋");
constexpr auto NRMember = Symbol("∌");

constexpr auto PSubset = Symbol("⊂");
constexpr auto NPSubset = Symbol("⊄");
constexpr auto Subset = Symbol("⊆");
constexpr auto NSubset = Symbol("⊈");
constexpr auto RPSubset = Symbol("⊃");
constexpr auto NRPSubset = Symbol("⊅");
constexpr auto RSubset = Symbol("⊇");
constexpr auto NRSubset = Symbol("⊉");

constexpr auto Prove = Symbol("⊢");
constexpr auto NProve = Symbol("⊬");
constexpr auto Valid = Symbol("⊨");
constexpr auto Invalid = Symbol("⊭");
constexpr auto Force = Symbol("⊩");
constexpr auto NForce = Symbol("⊮");

*/

/*

template <typename Down, typename Up, typename Carry>
static inline Formula traverse(const Formula& formula, Down&& down, Up&& up, Carry&& carry)
{
    vector<Formula> transformed;
    transformed.reserve(formula.size());
    for(const auto& subformula : formula) transformed.push_back(traverse(subformula, forward<Down>(down), forward<Up>(up), down(formula, carry)));
    return up(move(transformed), forward<Carry>(carry));
}

inline Formula alternating_normal_form(const Formula& formula) {}

inline Formula negative_normal_form(const Formula& formula)
{
    enum class odd_even : uint8_t
    {
        EVEN,
        ODD
    };

    const auto flip = [](const odd_even oe) -> odd_even
    {
        if(oe == odd_even::EVEN)
            return odd_even::ODD;
        else
            return odd_even::EVEN;
    };

    const auto down = [&flip](const Formula& formula, const odd_even negs) -> odd_even
    {
        if(formula.has_symbol(Not))
            return flip(negs);
        else
            return negs;
    };

    const auto up = [](vector<Formula>&& formula, const odd_even negs) -> Formula
    {
        if(negs == odd_even::EVEN)
            return Formula(And, move(formula));
        else
            return Formula(Or, move(formula));
    };

    return traverse(formula, move(down), move(up), odd_even::EVEN);
}

inline Formula conjunctive_normal_form(const Formula& formula) { const Formula nnf = negative_normal_form(formula); }

inline Formula disjunctive_normal_form(const Formula& formula) { const Formula nnf = negative_normal_form(formula); }

inline Formula algebraic_normal_form(const Formula& formula) {}

inline Formula prenex_normal_form(const Formula& formula) {}

inline Formula skolem_normal_form(const Formula& formula) { const Formula pnf = prenex_normal_form(formula); }

inline Formula herbrand_normal_form(const Formula& formula) { const Formula pnf = prenex_normal_form(formula); }

*/

} // namespace Logical

#ifdef DEBUG

namespace Logical
{

void formula_valuetypes_test(void)
{
	// logical_assert(type_name<typename Difference<Shadow<Unfold<Formula> >, Singleton<Formula> >::value_type>() == "Logical::Formula");
	logical_assert(type_name<typename Shadow<CompoundFormula>::value_type>() == "Logical::Formula");
	// logical_assert(type_name<typename Concat<Difference<Shadow<Unfold<Formula> >, Singleton<Formula> >, Shadow<CompoundFormula> >::value_type>() ==
	// "Logical::Formula");
}

void formula_test(void)
{
	formula_valuetypes_test();

	const auto a = ConnectiveSymbol("a");
	const auto b = ConnectiveSymbol("b");

	logical_assert(a == a);
	logical_assert(a != b);
	logical_assert(b != a);
	logical_assert(b == b);

	logical_assert(a() == a());
	logical_assert(a() != b());
	logical_assert(b() != a());
	logical_assert(b() == b());

	logical_assert(a().size() == 0);
	logical_assert(a().total_size() == 1);

	logical_assert(Or(a(), b()) == Or(a(), b()));
	logical_assert((a() & b()) == (a() & b()));

	const auto x = Variable("x");
	const auto y = Variable("y");
	const auto x_prim = Variable("x");

	logical_assert(Equal(x, y) == Equal(x, y));
	logical_assert(Equal(x, x) != Equal(y, y));

	const auto f1 = ForAll[x](Equal(x, x));
	const auto f2 = ForAll[y](Equal(y, y));
	const auto f1_prim = ForAll[x_prim](Equal(x, x_prim));

	logical_assert(f1 == f1_prim);
}

} // namespace Logical

#endif // DEBUG

#endif // LOGICAL_FORMULA_HH
