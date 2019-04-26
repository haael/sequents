
#ifndef LOGICAL_SEQUENT_HH
#define LOGICAL_SEQUENT_HH

#include "collections.hh"
#include "errors.hh"
#include "formula.hh"
#include "logical.hh"
#include "unionfind.hh"

namespace Logical
{

using std::pair;

static inline float fabs(float x)
{
	if(x >= 0)
		return x;
	else
		return -x;
}


static inline Shadow<CompoundFormula> ShadowOfCompoundFormula(const Formula& formula)
{
	return Shadow<CompoundFormula>(static_cast<const CompoundFormula&>(formula));
}


static inline Zip<CompoundFormula, CompoundFormula> ZipOfCompoundFormula(const Formula& first, const Formula& second)
{
	return Zip<CompoundFormula, CompoundFormula>(static_cast<const CompoundFormula&>(first), static_cast<const CompoundFormula&>(second));
}


class Sequent
{
private:
	class UnionFind;

	UnionFind* unionfind;
	bool toplevel;
	Unfold<Formula> left;
	Unfold<Formula> right;

	template<typename LeftInitializer, typename RightInitializer>
	Sequent(LeftInitializer&& l, RightInitializer&& r, UnionFind* uf)
	 : left(forward<LeftInitializer>(l))
	 , right(forward<RightInitializer>(r))
	 , unionfind(uf)
	 , toplevel(false)
	{
	}

protected:
	float guide_positive(const Formula& formula)
	{
		return formula.total_size();
	}

	float guide_negative(const Formula& formula)
	{
		return formula.total_size();
	}

	float guide_equal(const Formula& first, const Formula& second)
	{
		return (first.total_size() + second.total_size()) * (1.0f + fabs(float(first.total_size()) - float(second.total_size())));
	}

private:
	template <typename LeftInitializer, typename RightInitializer>
	static bool sub_prove(LeftInitializer&& l, RightInitializer&& r, UnionFind* uf)
	{
		return Sequent(forward<LeftInitializer>(l), forward<RightInitializer>(r), uf).prove();
	}

	bool breakdown(const Formula& formula)
	{
		//cerr << "breakdown: " << formula << endl;

		if(left.count(formula))
		{
			const auto singleton_formula = Singleton<Formula>(formula);
			const auto left_sans_formula = left - singleton_formula;

			logical_assert(left.count(formula));
			logical_assert(!left_sans_formula.count(formula));
			logical_assert(left_sans_formula.size() == left.size() - 1);
			logical_assert(Unfold<Formula>(left_sans_formula).size() == left_sans_formula.size());

			switch(formula.get_symbol())
			{
			case True:
				return sub_prove(left_sans_formula, right, unionfind);

			case False:
				return true;

			case Not:
				return sub_prove(left_sans_formula, right + Singleton<Formula>(formula[0]), unionfind);

			case RImpl:
				return ShadowOfCompoundFormula(formula).for_any([this, &left_sans_formula, &formula](auto& subformula) {
					if(&subformula == &formula[0])
						return sub_prove(left_sans_formula + Singleton<Formula>(formula[0]), right, unionfind);
					else if(&subformula == &formula[1])
						return sub_prove(left_sans_formula, right + Singleton<Formula>(formula[1]), unionfind);
					else
						throw RuntimeError("None of the implication subformulas identical to the formula provided.");
				});

			case Impl:
				return ShadowOfCompoundFormula(formula).for_any([this, &left_sans_formula, &formula](auto& subformula) {
					if(&subformula == &formula[1])
						return sub_prove(left_sans_formula + Singleton<Formula>(formula[1]), right, unionfind);
					else if(&subformula == &formula[0])
						return sub_prove(left_sans_formula, right + Singleton<Formula>(formula[0]), unionfind);
					else
						throw RuntimeError("None of the implication subformulas identical to the formula provided.");
				});

			case NRImpl:
				return sub_prove(left_sans_formula + Singleton<Formula>(formula[0]), right + Singleton<Formula>(formula[1]), unionfind);

			case NImpl:
				return sub_prove(left_sans_formula + Singleton<Formula>(formula[1]), right + Singleton<Formula>(formula[0]), unionfind);

			case And:
				return sub_prove(left_sans_formula + ShadowOfCompoundFormula(formula), right, unionfind);

			case Or:
				return ShadowOfCompoundFormula(formula)
				    .sort([this](const Formula& f) { return guide_negative(f); })
				    .for_all([this, &left_sans_formula, &formula](
				                 auto& subformula) { return sub_prove(left_sans_formula + Singleton<Formula>(subformula), right, unionfind); });

			case NOr:
				return sub_prove(left_sans_formula, right + ShadowOfCompoundFormula(formula), unionfind);

			case NAnd:
				return ShadowOfCompoundFormula(formula)
				    .sort([this](const Formula& f) { return guide_positive(f); })
				    .for_all([this, &left_sans_formula, &formula](
				                 auto& subformula) { return sub_prove(left_sans_formula, right + Singleton<Formula>(subformula), unionfind); });

			default:
				return false;
				// throw UnsupportedConnectiveError("Unsupported connective.", formula.get_symbol());
			}

			throw RuntimeError("Should not be here.");
		}

		if(right.count(formula))
		{
			const auto singleton_formula = Singleton<Formula>(formula);
			const auto right_sans_formula = right - singleton_formula;

			switch(formula.get_symbol())
			{
			case False:
				return sub_prove(left, right_sans_formula, unionfind);

			case True:
				return true;

			case Not:
				return sub_prove(left + Singleton<Formula>(formula[0]), right_sans_formula, unionfind);

			case NRImpl:
				return ShadowOfCompoundFormula(formula).for_any([this, &right_sans_formula, &formula](auto& subformula) {
					if(&subformula == &formula[0])
						return sub_prove(right_sans_formula + Singleton<Formula>(formula[0]), right, unionfind);
					else if(&subformula == &formula[1])
						return sub_prove(right_sans_formula, right + Singleton<Formula>(formula[1]), unionfind);
					else
						throw RuntimeError("None of the implication subformulas identical to the formula provided.");
				});

			case NImpl:
				return ShadowOfCompoundFormula(formula).for_any([this, &right_sans_formula, &formula](auto& subformula) {
					if(&subformula == &formula[1])
						return sub_prove(right_sans_formula + Singleton<Formula>(formula[1]), right, unionfind);
					else if(&subformula == &formula[0])
						return sub_prove(right_sans_formula, right + Singleton<Formula>(formula[0]), unionfind);
					else
						throw RuntimeError("None of the implication subformulas identical to the formula provided.");
				});

			case Impl:
				return sub_prove(left + Singleton<Formula>(formula[0]), right_sans_formula + Singleton<Formula>(formula[1]), unionfind);

			case RImpl:
				return sub_prove(left + Singleton<Formula>(formula[1]), right_sans_formula + Singleton<Formula>(formula[0]), unionfind);

			case Or:
				return sub_prove(left, right_sans_formula + ShadowOfCompoundFormula(formula), unionfind);

			case And:
				return ShadowOfCompoundFormula(formula)
				    .sort([this](const Formula& f) { return guide_positive(f); })
				    .for_all([this, &right_sans_formula, &formula](
				                 auto& subformula) { return sub_prove(left, right_sans_formula + Singleton<Formula>(subformula), unionfind); });

			case NAnd:
				return sub_prove(left + ShadowOfCompoundFormula(formula), right_sans_formula, unionfind);

			case NOr:
				return ShadowOfCompoundFormula(formula)
				    .sort([this](const Formula& f) { return guide_negative(f); })
				    .for_all([this, &right_sans_formula, &formula](
				                 auto& subformula) { return sub_prove(left + Singleton<Formula>(subformula), right_sans_formula, unionfind); });

			default:
				return false;
				// throw UnsupportedConnectiveError("Unsupported connective.", formula.get_symbol());
			}

			throw RuntimeError("Should not be here.");
		}

		throw RuntimeError("Formula not found on left nor right side of the sequent.");
	}

	bool equal(const Formula& first, const Formula& second)
	{
		//cerr << "equal: " << first << " == " << second << endl;
		if(unionfind)
			return unionfind->equal(first, second);
		else
			return formulas_equal(first, second);
	}

	bool formulas_equal(const Formula& first, const Formula& second)
	{
		static const auto commutative_symbols = unordered_set<Symbol, SymbolHash>({And, Or, NAnd, NOr, Xor, NXor, Equiv, NEquiv});
		static const auto idempotent_symbols = unordered_set<Symbol, SymbolHash>({And, Or, NAnd, NOr});

		const auto& first_symbol = first.get_symbol();
		const auto& second_symbol = second.get_symbol();

		if(first_symbol != second_symbol)
			return false;
		else if(first == second)
			return true;
		else if(commutative_symbols.count(first_symbol))
		{
			if(!idempotent_symbols.count(first_symbol) && first.size() != second.size())
				return false;

			const bool first_in_second = ShadowOfCompoundFormula(first).for_all([this, &second](const auto& sub1)
			{
				auto& parent = *this;
				return ShadowOfCompoundFormula(second)
				    .sort([&parent, &sub1](const auto& sub2) { return parent.guide_equal(sub1, sub2); })
				    .for_any([&parent, &sub1](const auto& sub2) { return parent.equal(sub1, sub2); });
			});

			const bool second_in_first = ShadowOfCompoundFormula(second).for_all([this, &first](const auto& sub2)
			{
				auto& parent = *this;
				return ShadowOfCompoundFormula(first)
				    .sort([&parent, &sub2](const auto& sub1) { return parent.guide_equal(sub2, sub1); })
				    .for_any([&parent, &sub2](const auto& sub1) { return parent.equal(sub2, sub1); });
			});

			return first_in_second && second_in_first;
		}
		else if(!first_symbol.is_relation() && !first_symbol.is_quantifier())
		{
			if(first.size() != second.size())
				return false;

			return ZipOfCompoundFormula(first, second)
			    .sort([this](const auto& p) { return -guide_equal(p.first, p.second); })
			    .for_all([this](const auto& p) { return equal(p.first, p.second); });
		}
		else if(!first_symbol.is_relation() && first_symbol.is_quantifier())
		{
			throw RuntimeError("Not implemented."); // TODO
		}
		else if(first_symbol.is_relation())
		{
			throw RuntimeError("Not implemented."); // TODO
		}
		else
		{
			throw RuntimeError("Unsupported case.");
		}
	}

	class UnionFind : public CompareCache<Formula>
	{
	private:
		Sequent& sequent;

	protected:
		bool value_compare(const Formula& one, const Formula& two)
		{
			return sequent.formulas_equal(one, two);
		}

	public:
		UnionFind(Sequent& s)
		 : sequent(s)
		{
		}
	};

public:
	template<typename LeftInitializer, typename RightInitializer>
	Sequent(LeftInitializer&& l, RightInitializer&& r, bool usecache=true)
	 : left(forward<LeftInitializer>(l))
	 , right(forward<RightInitializer>(r))
	 , unionfind(usecache ? new UnionFind(*this) : nullptr)
	 , toplevel(true)
	{
	}
	
	~Sequent(void)
	{
		if(unionfind && toplevel)
			delete unionfind;
	}
	
	bool prove(void)
	{
		//cerr << "prove " << (&left) << ", " << (&right) << endl;
		//cerr << left << " |- " << right << endl;
		
		return (left.size() == 0 && right.size() == 0)
		    || (left * right)
		           .sort([this](const pair<const Formula&, const Formula&>& p) { return guide_equal(p.first, p.second); })
		           .for_any([this](const pair<const Formula&, const Formula&>& p) { return equal(p.first, p.second); })
		    || (left + right)
		           .sort([this](const Formula& f) { return (left.count(f) ? guide_negative(f) : 0) + (right.count(f) ? guide_positive(f) : 0); })
		           .for_any([this](const Formula& f) { return breakdown(f); });
	}
};


inline bool prove(const initializer_list<Formula>& l, const initializer_list<Formula>& r)
{
	return Sequent(l, r).prove();
}

} // namespace Logical

#ifdef DEBUG

namespace Logical
{

void sequent_test(void)
{
	try
	{
		const auto a = ConnectiveSymbol("a");
		const auto b = ConnectiveSymbol("b");
		const auto c = ConnectiveSymbol("c");
		
		const auto ab = vector<Formula>({a(), b()});
		logical_assert(Unfold<Formula>(ab).sort([](const Formula& f) -> float { return f.total_size(); }).for_any([&a, &b](const Formula& f) -> bool { return f == b(); }));
		
        logical_assert(prove({}, {}), "Empty sequent should succeed.");
        logical_assert(prove({a()}, {a()}), "Sequent with the same symbol on both sides must succeed.");
        logical_assert(!prove({a()}, {b()}), "Sequent should fail.");
        logical_assert(prove({a()}, {b(), a()}), "Sequent should succeed.");
        logical_assert(prove({a(), b()}, {a()}), "Sequent should succeed.");
		
        logical_assert(!prove({}, {b()}), "Sequent should fail.");
        logical_assert(!prove({}, {a()}), "Sequent should fail.");
        logical_assert(!prove({Or(a(), b())}, {b()}), "Sequent should fail.");
        logical_assert(prove({And(a(), b())}, {a()}), "Sequent should succeed.");
        logical_assert(prove({}, {Or(a(), Not(a()))}), "Sequent should succeed.");
        logical_assert(prove({False()}, {False()}), "Sequent should succeed.");
        logical_assert(prove({}, {True()}), "Sequent should succeed.");
        logical_assert(prove({a(), Impl(a(), b())}, {b()}), "Sequent should succeed.");
        logical_assert(prove({Impl(a(), b())}, {Or(Not(a()), b())}), "Sequent should succeed.");
        logical_assert(prove({a()}, {True()}), "Sequent should succeed.");
        logical_assert(prove({a(), b()}, {a(), b()}), "Sequent should succeed.");
        logical_assert(prove({a(), b()}, {b(), a()}), "Sequent should succeed.");
        logical_assert(prove({a(), b()}, {And(a(), b())}), "Sequent should succeed.");
        logical_assert(prove({Impl(a(), b()), Impl(Not(a()), b())}, {b()}), "Sequent should succeed.");
        logical_assert(prove({Not(a()), a()}, {}), "Sequent should succeed.");
        logical_assert(prove({a()}, {a(), b()}), "Sequent should succeed.");
        logical_assert(prove({Impl(a(), b()), Impl(b(), c())}, {Impl(a(), c())}), "Sequent should succeed.");
        logical_assert(prove({Impl(a(), b()), Impl(a(), c())}, {Impl(a(), And(b(), c()))}), "Sequent should succeed.");
		
		logical_assert(!prove({Impl(a(), b())}, {Impl(b(), a())}), "Sequent of the form `a->b |- b->a` should fail.");
        logical_assert(prove({a() | b(), ~a()}, {b()}), "Sequent should succeed.");
		
		const auto x = Variable("x");
		const auto y = Variable("y");

		logical_assert(prove({Equal(x, x)}, {Equal(x, x)}));
		//logical_assert(!prove({Equal(x, x)}, {Equal(y, y)}));
	}
	catch(const UnsupportedConnectiveError& error)
	{
		cout << "UnsupportedConnectiveError.symbol = " << error.symbol << endl;
		throw error;
	}
}

} // namespace Logical

#endif // DEBUG

#endif // LOGICAL_SEQUENT_HH
