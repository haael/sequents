
#ifndef LOGICAL_HH
#define LOGICAL_HH

namespace Logical
{

template <typename Collection> class Iterator;
template <typename Item> class Empty;
template <typename Item> class Singleton;
template <typename Collection> class Shadow;
template <typename Collection1, typename Collection2> class Concat;
template <typename Collection1, typename Collection2> class Difference;
template <typename Collection1, typename Collection2> class Cartesian;
template <typename Collection> class Reorder;
template <typename Collection> class Parallel;

class UnionFind;
class Partition;

class Unifier;

class Symbol;
class Formula;

template <typename LeftShadow, typename RightShadow> class Sequent;

} // namespace Logical

#endif // LOGICAL_HH
