
#ifndef SEQUENT_HH
#define SEQUENT_HH




#include "logical.hh"
#include "formula.hh"
#include "collections.hh"
#include "unionfind.hh"


namespace Logical {


using std::pair;


static inline float fabs(float x)
{
	if(x >= 0) return x;
	else return -x;
}


static mutex print_mutex;

template<typename LeftShadow, typename RightShadow>
class Sequent
{
private:
	class UnionFind;
	
	UnionFind* unionfind;
	bool toplevel;
	Shadow<LeftShadow> left;
	Shadow<RightShadow> right;
	
	Sequent(const LeftShadow& l, const RightShadow& r, UnionFind* uf)
	 : left(l), right(r), unionfind(uf), toplevel(false)
	{
	}

protected:
	float guide_positive(const Formula& formula)
	{
		return -formula.total_size();
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
	template<typename Left, typename Right>
	static bool sub_prove(Left&& l, Right&& r, UnionFind* uf)
	{
		const auto ll = Unfold<Formula>(l);
		const auto rr = Unfold<Formula>(r);
		auto s = Sequent<Unfold<Formula>, Unfold<Formula>>(ll, rr, uf);
		return s.prove();
	}
	
	bool breakdown(const Formula& formula)
	{
		/*
		{
			lock_guard<mutex> lock(print_mutex);
			std::cerr << "breakdown: " << Unfold<Formula>(left) << " " << Unfold<Formula>(right) << " " << formula << std::endl;
			std::cerr << "left =";
			for(const auto& f : left)
				std::cerr << " " << (&f);
			std::cerr << std::endl;
			std::cerr << "right =";
			for(const auto& f : right)
				std::cerr << " " << (&f);
			std::cerr << std::endl;
			std::cerr << "formula = " << (&formula) << std::endl;
		}
		*/
		
		//if(left.count(formula, [this](const Formula& one, const Formula& two) -> bool { return equal(one, two); }))
		if(left.count(formula))
		{
			const auto singleton_formula = Singleton(formula);
			const auto left_sans_formula = left - singleton_formula;

/*
			std::cout << left.count(formula) << std::endl;
			std::cout << left_sans_formula.count(formula) << std::endl;
			std::cout << left_sans_formula.size() << std::endl;

			//const auto ufx = Unfold<Formula>(left_sans_formula);
			//ufx.print(std::cout);
			//std::cout << ufx.size() << std::endl;
			std::cout << Unfold<Formula>(left_sans_formula).size() << std::endl;

			std::cout << left_sans_formula.count(formula) << std::endl;
			std::cout << left_sans_formula.size() << std::endl;
			std::cout << left.size() << std::endl;
			std::cout << Unfold<Formula>(left_sans_formula).size() << std::endl;
*/

			assert(left.count(formula));
			assert(!left_sans_formula.count(formula));
			assert(left_sans_formula.size() == left.size() - 1);
			assert(Unfold<Formula>(left_sans_formula).size() == left_sans_formula.size());

/*			
			std::cout << "formula = " << formula << " @" << (&formula) << ";";
			std::cout << " (" << left_sans_formula.size() << ") left_sans_formula = ";
			Unfold<Formula>(left_sans_formula).print_with_addresses(std::cout);
			std::cout << ";";
			std::cout << " (" << left.size() << ") left = ";
			Unfold<Formula>(left).print_with_addresses(std::cout);
			std::cout << ";";
			std::cout << std::endl;
*/
		
			switch(formula.get_symbol())
			{
			case True:
				return sub_prove(left_sans_formula, right, unionfind);
			
			case False:
				return true;
			
			case Not:
				return sub_prove(left_sans_formula, right + Singleton(formula[0]), unionfind);
			
			case RImpl:
				return Shadow(formula)
					.sort([this](const Formula& f) { return guide_negative(f); })
					.for_any([this, &left_sans_formula, &formula](auto& subformula)
					{
						if(&subformula == &formula[0])
							return sub_prove(left_sans_formula + Singleton(formula[0]), right, unionfind);
						else if(&subformula == &formula[1])
							return sub_prove(left_sans_formula, right + Singleton(formula[1]), unionfind);
						else
							throw RuntimeError("None of the implication subformulas identical to the formula provided.");
					});
			
			case Impl:
				return Shadow(formula)
					.sort([this](const Formula& f) { return guide_negative(f); })
					.for_any([this, &left_sans_formula, &formula](auto& subformula)
					{
						if(&subformula == &formula[1])
							return sub_prove(left_sans_formula + Singleton(formula[1]), right, unionfind);
						else if(&subformula == &formula[0])
							return sub_prove(left_sans_formula, right + Singleton(formula[0]), unionfind);
						else
							throw RuntimeError("None of the implication subformulas identical to the formula provided.");
					});
			
			case NRImpl:
				return sub_prove(left_sans_formula + Singleton(formula[0]), right + Singleton(formula[1]), unionfind);
			
			case NImpl:
				return sub_prove(left_sans_formula + Singleton(formula[1]), right + Singleton(formula[0]), unionfind);
			
			case And:
				return sub_prove(left_sans_formula + Shadow(formula), right, unionfind);
			
			case Or:
				return Shadow(formula)
					.sort([this](const Formula& f) { return guide_negative(f); })
					.run_parallel(false, [this, &left_sans_formula, &formula](auto& subformula)
					{
						return sub_prove(left_sans_formula + Singleton(subformula), right, unionfind);
					});
			
			case NOr:
				return sub_prove(left_sans_formula, right + Shadow(formula), unionfind);
			
			case NAnd:
				return Shadow(formula)
					.sort([this](const Formula& f) { return guide_negative(f); })
					.run_parallel(false, [this, &left_sans_formula, &formula](auto& subformula)
					{
						return sub_prove(left_sans_formula, right + Singleton(subformula), unionfind);
					});
			
			default:
				return false;
			}
			
			throw RuntimeError("Should not be here.");
		}
		
		//if(right.count(formula, [this](const Formula& one, const Formula& two) -> bool { return equal(one, two); }))
		if(right.count(formula))
		{
			const auto singleton_formula = Singleton(formula);
			const auto right_sans_formula = right - singleton_formula;

			/*{
				lock_guard<mutex> lock(print_mutex);
				std::cout << "  right: " <<  formula << std::endl;
			}*/
			
			switch(formula.get_symbol())
			{
			case False:
				return sub_prove(left, right_sans_formula, unionfind);
			
			case True:
				return true;
			
			case Not:
				return sub_prove(left + Singleton(formula[0]), right_sans_formula, unionfind);
			
			case NRImpl:
				return Shadow(formula)
					.sort([this](const Formula& f) { return guide_negative(f); })
					.for_any([this, &right_sans_formula, &formula](auto& subformula)
					{
						if(&subformula == &formula[0])
							return sub_prove(right_sans_formula + Singleton(formula[0]), right, unionfind);
						else if(&subformula == &formula[1])
							return sub_prove(right_sans_formula, right + Singleton(formula[1]), unionfind);
						else
							throw RuntimeError("None of the implication subformulas identical to the formula provided.");
					});
			
			case NImpl:
				return Shadow(formula)
					.sort([this](const Formula& f) { return guide_negative(f); })
					.for_any([this, &right_sans_formula, &formula](auto& subformula)
					{
						if(&subformula == &formula[1])
							return sub_prove(right_sans_formula + Singleton(formula[1]), right, unionfind);
						else if(&subformula == &formula[0])
							return sub_prove(right_sans_formula, right + Singleton(formula[0]), unionfind);
						else
							throw RuntimeError("None of the implication subformulas identical to the formula provided.");
					});
			
			case Impl:
				return sub_prove(left + Singleton(formula[0]), right_sans_formula + Singleton(formula[1]), unionfind);
			
			case RImpl:
				return sub_prove(left + Singleton(formula[1]), right_sans_formula + Singleton(formula[0]), unionfind);
			
			case Or:
				return sub_prove(left, right_sans_formula + Shadow(formula), unionfind);
			
			case And:
				return Shadow(formula)
					.sort([this](const Formula& f) { return guide_negative(f); })
					.run_parallel(false, [this, &right_sans_formula, &formula](auto& subformula)
					{
						return sub_prove(left, right_sans_formula + Singleton(subformula), unionfind);
					});
			
			case NOr:
				return sub_prove(left + Shadow(formula), right_sans_formula, unionfind);
			
			case NAnd:
				return Shadow(formula)
					.sort([this](const Formula& f) { return guide_negative(f); })
					.run_parallel(false, [this, &right_sans_formula, &formula](auto& subformula)
					{
						return sub_prove(left + Singleton(subformula), right_sans_formula, unionfind);
					});
			
			default:
				return false;
			}
			
			throw RuntimeError("Should not be here.");
		}
		
		throw RuntimeError("Formula not found on left nor right side of the sequent.");
	}
	
	bool equal(const Formula& first, const Formula& second)
	{
		if(unionfind)
			return unionfind->equal(first, second);
		else
			return formulas_equal(first, second);
	}
	
	bool formulas_equal(const Formula& first, const Formula& second)
	{
		if(first.get_symbol() != second.get_symbol())
			return false;
		else if(first == second)
			return true;
		else
		{
			const bool first_in_second =
				Shadow(first)
				.run_parallel(false, [this, &second](const auto& sub1)
				{
					auto& parent = *this;
					return Shadow(second)
						.sort([&parent, &sub1](const auto& sub2)
						{
							return parent.guide_equal(sub1, sub2); 
						})
						.run_parallel(true, [&parent, &sub1](const auto& sub2)
						{
							return parent.equal(sub1, sub2);
						});
				});
			
			const bool second_in_first =
				Shadow(second)
				.run_parallel(false, [this, &first](const auto& sub2)
				{
					auto& parent = *this;
					return Shadow(first)
						.sort([&parent, &sub2](const auto& sub1)
						{
							return parent.guide_equal(sub2, sub1);
						})
						.run_parallel(true, [&parent, &sub2](const auto& sub1)
						{
							return parent.equal(sub2, sub1);
						});
				});
			
			return first_in_second && second_in_first;
		}
		
		throw RuntimeError("Unreachable code.");
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
	Sequent(const initializer_list<Formula>& l, const initializer_list<Formula>& r, bool usecache = false)
	 : left(l), right(r), unionfind(usecache ? new UnionFind(*this) : nullptr), toplevel(true)
	{
	}
	
	Sequent(const LeftShadow& l, const RightShadow& r, bool usecache = true)
	 : left(l), right(r), unionfind(usecache ? new UnionFind(*this) : nullptr), toplevel(true)
	{
	}
	
	~Sequent(void)
	{
		if(toplevel && unionfind)
			delete unionfind;
	}
	
	//mutex print_mutex;
	
	bool prove(void)
	{
		/*{
			lock_guard<mutex> lock(print_mutex);
			std::cout << "prove " << Unfold<Formula>(left) << " |- " << Unfold<Formula>(right) << std::endl;
		}*/
		
		return
		//const bool result = 
		 (left.size() == 0 && right.size() == 0)
		  ||
		 (left * right)
		  .sort([this](const pair<const Formula&, const Formula&>& p) { return guide_equal(p.first, p.second); })
		  .for_any([this](const pair<const Formula&, const Formula&>& p)
			{
				//lock_guard<mutex> lock(print_mutex);
				//std::cout << " equal " << p.first << " == " << p.second << std::endl;
				return equal(p.first, p.second);
			})
		  ||
		 (left + right)
		  .sort([this](const Formula& f) { return guide_positive(f); })
		  .for_any([this](const Formula& f) { return breakdown(f); });
		
		//std::cout << result << std::endl;
		//return result;
	}
};

bool prove(const initializer_list<Formula>& l, const initializer_list<Formula>& r)
{
	const auto ll = Unfold<Formula>(l);
	const auto rr = Unfold<Formula>(r);
	auto s = Sequent<Unfold<Formula>, Unfold<Formula>>(ll, rr);
	return s.prove();
}


} // namespace Logical


#ifdef DEBUG

namespace Logical
{

void sequent_test(void)
{
	const auto a = Symbol("a");
	const auto b = Symbol("b");
	const auto c = Symbol("c");

	assert(Unfold<Formula>({a(), b()}).sort([](const Formula& f) -> float { return f.total_size(); }).for_any([&a, &b](const Formula& f) -> bool { return f == b(); }));

	assert(prove({}, {}), "Empty sequent should succeed.");
	assert(prove({a()}, {a()}), "Sequent with the same symbol on both sides must succeed.");
	assert(!prove({a()}, {b()}), "Sequent should fail.");
	assert(prove({a()}, {b(), a()}), "Sequent should succeed.");
	assert(prove({a(), b()}, {a()}), "Sequent should succeed.");
	assert(!prove({}, {b()}), "Sequent should fail.");
	assert(!prove({}, {a()}), "Sequent should fail.");
	assert(!prove({Or(a(), b())}, {b()}), "Sequent should fail.");
	assert(prove({And(a(), b())}, {a()}), "Sequent should succeed.");
	assert(prove({}, {Or(a(), Not(a()))}), "Sequent should succeed.");
	assert(prove({False()}, {False()}), "Sequent should succeed.");
	assert(prove({}, {True()}), "Sequent should succeed.");
	assert(prove({a(), Impl(a(), b())}, {b()}), "Sequent should succeed.");
	assert(prove({Impl(a(), b())}, {Or(Not(a()), b())}), "Sequent should succeed.");
	assert(prove({a()}, {True()}), "Sequent should succeed.");
	assert(prove({a(), b()}, {a(), b()}), "Sequent should succeed.");
	assert(prove({a(), b()}, {b(), a()}), "Sequent should succeed.");
	assert(prove({a(), b()}, {And(a(), b())}), "Sequent should succeed.");
	assert(prove({Impl(a(), b()), Impl(Not(a()), b())}, {b()}), "Sequent should succeed.");
	assert(prove({Not(a()), a()}, {}), "Sequent should succeed.");
	assert(prove({a()}, {a(), b()}), "Sequent should succeed.");
	assert(prove({Impl(a(), b()), Impl(b(), c())}, {Impl(a(), c())}), "Sequent should succeed.");
	assert(prove({Impl(a(), b()), Impl(a(), c())}, {Impl(a(), And(b(), c()))}), "Sequent should succeed.");
}

} // namespace Logical

#endif // DEBUG


#endif // SEQUENT_HH
