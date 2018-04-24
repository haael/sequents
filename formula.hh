
#ifndef FORMULA_HH
#define FORMULA_HH

#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace Logical
{

using std::cout;
using std::endl;
using std::ostream;
using std::string_view;
using std::vector;

class Symbol;
class Formula;

class Symbol
{
private:
	string_view value;

public:	
	constexpr Symbol(const string_view& s);
	Symbol(const Symbol& s);
	Symbol(Symbol&& s);

	template <typename... Args>
	Formula operator()(Args&&... args) const;

	void print(ostream& out) const;

	const string_view& get_value(void) const;
	constexpr uint64_t hash(uint64_t seed = 0) const;
	constexpr operator uint64_t (void) const;
	
	bool operator==(const Symbol& that) const { return this == &that || &value == &that.value || value == that.value; }
	bool operator!=(const Symbol& that) const { return !((*this) == that); }
};

class Formula
{
private:
	const Symbol& symbol;
	const vector<Formula> formula;

public:
	typedef Formula value_type;

	//template <typename FormulaInit> Formula(FormulaInit&& f);
	Formula(const Formula&);
	Formula(Formula&&);
	template <typename SymbolInit, typename VectorInit> Formula(SymbolInit&& s, VectorInit&& v);

	void print(ostream& out) const;

	uint64_t hash(uint64_t seed = 0) const
	{
		seed ^= symbol.hash(seed);
		
		for(const auto& f : formula)
		{
			seed ^= f.hash(seed);
		}
		
		return seed;
	}

	bool operator==(const Formula& that) const
	{
#ifdef DEBUG
		if(!this)
			throw RuntimeError("'this' pointer is null");
		if(!&that)
			throw RuntimeError("'&that' pointer is null");
#endif
		std::cerr << "Formula compare: " << this << "  ?= " << &that << std::endl;
		return (this == &that) || ((symbol == that.symbol) && (formula == that.formula));
	}

	bool operator!=(const Formula& that) const { return (this != &that) && ((symbol != that.symbol) || (formula != that.formula)); }

	const Symbol& get_symbol(void) const { return symbol; }

	bool has_symbol(const Symbol& s) const { return s == symbol; }

	template <typename Symbols>
	bool has_symbols(const Symbols& ss)
	{
		for(const Symbol& s : ss)
			if(s == symbol) return true;
		return false;
	}

	size_t size(void) const { return formula.size(); }

	const Formula& operator[](const size_t index) const
	{
		if(index > formula.size()) throw IndexError("Sub-formula index out of range.", index, formula.size());
		return formula[index];
	}

	auto begin(void) const { return formula.begin(); }

	auto end(void) const { return formula.end(); }

	size_t total_size(void) const
	{
		size_t s = 1;
		for(const auto& f : formula) s += f.total_size();
		return s;
	}

	size_t depth(void) const
	{
		size_t d = 0;
		for(const auto& f : formula)
		{
			const size_t nd = f.depth();
			if(nd > d) d = nd;
		}
		return d + 1;
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

inline constexpr Symbol::Symbol(const string_view& s)
 : value(s)
{
}

inline Symbol::Symbol(const Symbol& s)
 : value(s.value)
{
}

inline Symbol::Symbol(Symbol&& s)
 : value(move(s.value))
{
}

template <typename... Args>
inline Formula Symbol::operator()(Args&&... args) const
{
	return Formula(*this, vector<Formula>{forward<Args>(args)...});
}

inline constexpr uint64_t Symbol::hash(uint64_t seed) const
{
	for(char c : value)
	{
		seed = (257 * seed + (unsigned char)c + 13) ^ (seed >> (64 - 8));
	}

	return seed;
}

inline constexpr Symbol::operator uint64_t (void) const
{
	return hash(0x38a10a1c);
}

inline void Symbol::print(ostream& out) const { out << value; }

inline Formula::Formula(const Formula& f)
 : symbol(f.symbol)
 , formula(f.formula)
{
	//std::cout << "copy formula: " << (*this) << ", " << (const void*)(&f) << "->" << (void*)this << std::endl;
}

inline Formula::Formula(Formula&& f)
 : symbol(move(f.symbol))
 , formula(move(f.formula))
{
	//std::cout << "move formula: " << (*this) << ", " << (void*)(&f) << "->" << (void*)this << std::endl;
}

template <typename SymbolInit, typename VectorInit>
inline Formula::Formula(SymbolInit&& s, VectorInit&& v)
 : symbol(forward<SymbolInit>(s))
 , formula(forward<VectorInit>(v))
{
	//std::cout << "create formula: " << (*this) << ", " << (void*)this << std::endl;
}

inline void Formula::print(ostream& out) const
{
	out << symbol;
	out << "(";
	bool first = true;
	for(auto& f : formula)
	{
		if(first)
			first = false;
		else
			out << ",";
		out << f;
	}
	out << ")";
}


constexpr Symbol Not = Symbol("¬");

constexpr Symbol And = Symbol("∧");
constexpr Symbol Or = Symbol("∨");
constexpr Symbol NAnd = Symbol("⊼");
constexpr Symbol NOr = Symbol("⊽");

constexpr Symbol Xor = Symbol("⊻");
constexpr Symbol NXor = Symbol("⩝");
constexpr Symbol Eql = Symbol("↔");
constexpr Symbol NEql = Symbol("↮");

constexpr Symbol Impl = Symbol("→");
constexpr Symbol NImpl = Symbol("↛");
constexpr Symbol RImpl = Symbol("←");
constexpr Symbol NRImpl = Symbol("↚");

constexpr Symbol ForAll = Symbol("∀");
constexpr Symbol Exists = Symbol("∃");
constexpr Symbol Unique = Symbol("∃!");

constexpr Symbol True = Symbol("⊤");
constexpr Symbol False = Symbol("⊥");

constexpr Symbol Equal = Symbol("=");
constexpr Symbol NEqual = Symbol("≠");

constexpr Symbol Everyone = Symbol("◻");
constexpr Symbol Someone = Symbol("◇");

constexpr Symbol WillAlways = Symbol("⟥");
constexpr Symbol WillOnce = Symbol("⟣");
constexpr Symbol WasAlways = Symbol("⟤");
constexpr Symbol WasOnce = Symbol("⟢");


/*

constexpr Symbol Id = Symbol("⍳");

constexpr Symbol Assert = Symbol("⇶");
constexpr Symbol Dissert = Symbol("↯");

constexpr Symbol Lesser = Symbol("<");
constexpr Symbol NLesser = Symbol("≥");
constexpr Symbol Greater = Symbol(">");
constexpr Symbol NGreater = Symbol("≤");

constexpr Symbol Member = Symbol("∈");
constexpr Symbol NMember = Symbol("∉");
constexpr Symbol RMember = Symbol("∋");
constexpr Symbol NRMember = Symbol("∌");

constexpr Symbol PSubset = Symbol("⊂");
constexpr Symbol NPSubset = Symbol("⊄");
constexpr Symbol Subset = Symbol("⊆");
constexpr Symbol NSubset = Symbol("⊈");
constexpr Symbol RPSubset = Symbol("⊃");
constexpr Symbol NRPSubset = Symbol("⊅");
constexpr Symbol RSubset = Symbol("⊇");
constexpr Symbol NRSubset = Symbol("⊉");

constexpr Symbol Prove = Symbol("⊢");
constexpr Symbol NProve = Symbol("⊬");
constexpr Symbol Valid = Symbol("⊨");
constexpr Symbol Invalid = Symbol("⊭");
constexpr Symbol Force = Symbol("⊩");
constexpr Symbol NForce = Symbol("⊮");

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

void formula_test(void)
{
	const auto a = Symbol("a");
	const auto b = Symbol("b");
	
	assert(a == a);
	assert(a != b);
	assert(b != a);
	assert(b == b);

	assert(a() == a());
	assert(a() != b());
	assert(b() != a());
	assert(b() == b());


	assert(a().size() == 0);
	assert(a().total_size() == 1);
	
	assert(Or(a(), b()) == Or(a(), b()));
}

} // namespace Logical

#endif // DEBUG


#endif // FORMULA_HH
