
#ifndef LOGICAL_LOGICAL_HH
#define LOGICAL_LOGICAL_HH

namespace Logical
{

template <typename Collection>
class Iterator;
template <typename Item>
class Empty;
template <typename Item>
class Singleton;
template <typename Collection>
class Shadow;
template <typename Collection1, typename Collection2>
class Concat;
template <typename Collection1, typename Collection2>
class Difference;
template <typename Collection1, typename Collection2>
class Cartesian;
template <typename Collection1, typename Collection2>
class Zip;
template <typename Collection>
class Reorder;
template <typename Collection>
class Parallel;

class UnionFind;
class Partition;

class Unifier;

class Expression;
class ExpressionReference;
class ExpressionIterator;
class Variable;

class Symbol;
class ConnectiveSymbol;
class RelationSymbol;
class Formula;
class CompoundFormula;
class AtomicFormula;

class Sequent;

} // namespace Logical

#endif // LOGICAL_LOGICAL_HH
