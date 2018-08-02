/*
    Copyright 2005-2014 Intel Corporation.  All Rights Reserved.

    This file is part of Threading Building Blocks. Threading Building Blocks is free software;
    you can redistribute it and/or modify it under the terms of the GNU General Public License
    version 2  as  published  by  the  Free Software Foundation.  Threading Building Blocks is
    distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the
    implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
    See  the GNU General Public License for more details.   You should have received a copy of
    the  GNU General Public License along with Threading Building Blocks; if not, write to the
    Free Software Foundation, Inc.,  51 Franklin St,  Fifth Floor,  Boston,  MA 02110-1301 USA

    As a special exception,  you may use this file  as part of a free software library without
    restriction.  Specifically,  if other files instantiate templates  or use macros or inline
    functions from this file, or you compile this file and link it with other files to produce
    an executable,  this file does not by itself cause the resulting executable to be covered
    by the GNU General Public License. This exception does not however invalidate any other
    reasons why the executable file might be covered by the GNU General Public License.
*/

#ifndef __TBB_partitioner_H
#define __TBB_partitioner_H

#ifndef __TBB_INITIAL_CHUNKS
// initial task divisions per thread
#define __TBB_INITIAL_CHUNKS 2
#endif
#ifndef __TBB_RANGE_POOL_CAPACITY
// maximum number of elements in range pool
#define __TBB_RANGE_POOL_CAPACITY 8
#endif
#ifndef __TBB_INIT_DEPTH
// initial value for depth of range pool
#define __TBB_INIT_DEPTH 5
#endif
#ifndef __TBB_DEMAND_DEPTH_ADD
// when imbalance is found range splits this value times more
#define __TBB_DEMAND_DEPTH_ADD 2
#endif
#ifndef __TBB_STATIC_THRESHOLD
// necessary number of clocks for the work to be distributed among all tasks
#define __TBB_STATIC_THRESHOLD 40000
#endif
#if __TBB_DEFINE_MIC
#define __TBB_NONUNIFORM_TASK_CREATION 1
#ifdef __TBB_machine_time_stamp
#define __TBB_USE_MACHINE_TIME_STAMPS 1
#define __TBB_task_duration() __TBB_STATIC_THRESHOLD
#endif // __TBB_machine_time_stamp
#endif // __TBB_DEFINE_MIC

#include "task.h"
#include "aligned_space.h"
#include "atomic.h"

#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
    // Workaround for overzealous compiler warnings
    #pragma warning (push)
    #pragma warning (disable: 4244)
#endif

namespace tbb {

class auto_partitioner;
class simple_partitioner;
class affinity_partitioner;
namespace interface7 {
    namespace internal {
        class affinity_partition_type;
    }
}

namespace internal { //< @cond INTERNAL
size_t __TBB_EXPORTED_FUNC get_initial_auto_partitioner_divisor();

//! Defines entry point for affinity partitioner into tbb run-time library.
class affinity_partitioner_base_v3: no_copy {
    friend class tbb::affinity_partitioner;
    friend class tbb::interface7::internal::affinity_partition_type;
    //! Array that remembers affinities of tree positions to affinity_id.
    /** NULL if my_size==0. */
    affinity_id* my_array;
    //! Number of elements in my_array.
    size_t my_size;
    //! Zeros the fields.
    affinity_partitioner_base_v3() : my_array(NULL), my_size(0) {}
    //! Deallocates my_array.
    ~affinity_partitioner_base_v3() {resize(0);}
    //! Resize my_array.
    /** Retains values if resulting size is the same. */
    void __TBB_EXPORTED_METHOD resize( unsigned factor );
};

//! Provides backward-compatible methods for partition objects without affinity.
class partition_type_base {
public:
    void set_affinity( task & ) {}
    void note_affinity( task::affinity_id ) {}
    task* continue_after_execute_range() {return NULL;}
    bool decide_whether_to_delay() {return false;}
    void spawn_or_delay( bool, task& b ) {
        task::spawn(b);
    }
};

template<typename Range, typename Body, typename Partitioner> class start_scan;

} //< namespace internal @endcond

namespace serial {
namespace interface7 {
template<typename Range, typename Body, typename Partitioner> class start_for;
}
}

namespace interface7 {
//! @cond INTERNAL
namespace internal {
using namespace tbb::internal;
template<typename Range, typename Body, typename Partitioner> class start_for;
template<typename Range, typename Body, typename Partitioner> class start_reduce;

//! Join task node that contains shared flag for stealing feedback
class flag_task: public task {
public:
    tbb::atomic<bool> my_child_stolen;
    flag_task() { my_child_stolen = false; }
    task* execute() { return NULL; }
    static void mark_task_stolen(task &t) {
        tbb::atomic<bool> &flag = static_cast<flag_task*>(t.parent())->my_child_stolen;
#if TBB_USE_THREADING_TOOLS
        // Threading tools respect lock prefix but report false-positive data-race via plain store
        flag.fetch_and_store<release>(true);
#else
        flag = true;
#endif //TBB_USE_THREADING_TOOLS
    }
    static bool is_peer_stolen(task &t) {
        return static_cast<flag_task*>(t.parent())->my_child_stolen;
    }
};

//! Depth is a relative depth of recursive division inside a range pool. Relative depth allows
//! infinite absolute depth of the recursion for heavily unbalanced workloads with range represented
//! by a number that cannot fit into machine word.
typedef unsigned char depth_t;

//! Range pool stores ranges of type T in a circular buffer with MaxCapacity
template <typename T, depth_t MaxCapacity>
class range_vector {
    depth_t my_head;
    depth_t my_tail;
    depth_t my_size;
    depth_t my_depth[MaxCapacity]; // relative depths of stored ranges
    tbb::aligned_space<T, MaxCapacity> my_pool;

public:
    //! initialize via first range in pool
    range_vector(const T& elem) : my_head(0), my_tail(0), my_size(1) {
        my_depth[0] = 0;
        new( static_cast<void *>(my_pool.begin()) ) T(elem);//TODO: std::move?
    }
    ~range_vector() {
        while( !empty() ) pop_back();
    }
    bool empty() const { return my_size == 0; }
    depth_t size() const { return my_size; }
    //! Populates range pool via ranges up to max depth or while divisible
    //! max_depth starts from 0, e.g. value 2 makes 3 ranges in the pool up to two 1/4 pieces
    void split_to_fill(depth_t max_depth) {
        while( my_size < MaxCapacity && is_divisible(max_depth) ) {
            depth_t prev = my_head;
            my_head = (my_head + 1) % MaxCapacity;
            new(my_pool.begin()+my_head) T(my_pool.begin()[prev]); // copy TODO: std::move?
            my_pool.begin()[prev].~T(); // instead of assignment
            new(my_pool.begin()+prev) T(my_pool.begin()[my_head], split()); // do 'inverse' split
            my_depth[my_head] = ++my_depth[prev];
            my_size++;
        }
    }
    void pop_back() {
        __TBB_ASSERT(my_size > 0, "range_vector::pop_back() with empty size");
        my_pool.begin()[my_head].~T();
        my_size--;
        my_head = (my_head + MaxCapacity - 1) % MaxCapacity;
    }
    void pop_front() {
        __TBB_ASSERT(my_size > 0, "range_vector::pop_front() with empty size");
        my_pool.begin()[my_tail].~T();
        my_size--;
        my_tail = (my_tail + 1) % MaxCapacity;
    }
    T& back() {
        __TBB_ASSERT(my_size > 0, "range_vector::back() with empty size");
        return my_pool.begin()[my_head];
    }
    T& front() {
        __TBB_ASSERT(my_size > 0, "range_vector::front() with empty size");
        return my_pool.begin()[my_tail];
    }
    //! similarly to front(), returns depth of the first range in the pool
    depth_t front_depth() {
        __TBB_ASSERT(my_size > 0, "range_vector::front_depth() with empty size");
        return my_depth[my_tail];
    }
    depth_t back_depth() {
        __TBB_ASSERT(my_size > 0, "range_vector::back_depth() with empty size");
        return my_depth[my_head];
    }
    bool is_divisible(depth_t max_depth) {
        return back_depth() < max_depth && back().is_divisible();
    }
};

//! Provides default methods for partition objects and common algorithm blocks.
template <typename Partition>
struct partition_type_base {
    typedef split split_type;
    // decision makers
    void set_affinity( task & ) {}
    void note_affinity( task::affinity_id ) {}
    bool check_being_stolen(task &) { return false; } // part of old should_execute_range()
    bool check_for_demand(task &) { return false; }
    bool is_divisible() { return true; } // part of old should_execute_range()
    depth_t max_depth() { return 0; }
    void align_depth(depth_t) { }
    template <typename Range> split_type get_split() { return split(); }

    // common function blocks
    Partition& self() { return *static_cast<Partition*>(this); } // CRTP helper
    template<typename StartType, typename Range>
    void execute(StartType &start, Range &range) {
        // The algorithm in a few words ([]-denotes calls to decision methods of partitioner):
        // [If this task is stolen, adjust depth and divisions if necessary, set flag].
        // If range is divisible {
        //    Spread the work while [initial divisions left];
        //    Create trap task [if necessary];
        // }
        // If not divisible or [max depth is reached], execute, else do the range pool part
        if ( range.is_divisible() ) {
            if ( self().is_divisible() ) {
                do { // split until is divisible
                    typename Partition::split_type split_obj = self().template get_split<Range>();
                    start.offer_work( split_obj );
                } while ( range.is_divisible() && self().is_divisible() );
            }
        }
        if( !range.is_divisible() || !self().max_depth() )
            start.run_body( range ); // simple partitioner goes always here
        else { // do range pool
            internal::range_vector<Range, Partition::range_pool_size> range_pool(range);
            do {
                range_pool.split_to_fill(self().max_depth()); // fill range pool
                if( self().check_for_demand( start ) ) {
                    if( range_pool.size() > 1 ) {
                        start.offer_work( range_pool.front(), range_pool.front_depth() );
                        range_pool.pop_front();
                        continue;
                    }
                    if( range_pool.is_divisible(self().max_depth()) ) // was not enough depth to fork a task
                        continue; // note: next split_to_fill() should split range at least once
                }
                start.run_body( range_pool.back() );
                range_pool.pop_back();
            } while( !range_pool.empty() && !start.is_cancelled() );
        }
    }
};

//! Provides default methods for auto (adaptive) partition objects.
template <typename Partition>
struct adaptive_partition_type_base : partition_type_base<Partition> {
    size_t my_divisor;
    depth_t my_max_depth;
    adaptive_partition_type_base() : my_max_depth(__TBB_INIT_DEPTH) {
        my_divisor = tbb::internal::get_initial_auto_partitioner_divisor() / 4;
        __TBB_ASSERT(my_divisor, "initial value of get_initial_auto_partitioner_divisor() is not valid");
    }
    adaptive_partition_type_base(adaptive_partition_type_base &src, split) {
        my_max_depth = src.my_max_depth;
#if TBB_USE_ASSERT
        size_t old_divisor = src.my_divisor;
#endif

#if __TBB_INITIAL_TASK_IMBALANCE
        if( src.my_divisor <= 1 ) my_divisor = 0;
        else my_divisor = src.my_divisor = (src.my_divisor + 1u) / 2u;
#else
        my_divisor = src.my_divisor / 2u;
        src.my_divisor = src.my_divisor - my_divisor; // TODO: check the effect separately
        if (my_divisor) src.my_max_depth += static_cast<depth_t>(__TBB_Log2(src.my_divisor / my_divisor));
#endif
        // For affinity_partitioner, my_divisor indicates the number of affinity array indices the task reserves.
        // A task which has only one index must produce the right split without reserved index in order to avoid
        // it to be overwritten in note_affinity() of the created (right) task.
        // I.e. a task created deeper than the affinity array can remember must not save its affinity (LIFO order)
        __TBB_ASSERT( (old_divisor <= 1 && my_divisor == 0) ||
                      (old_divisor > 1 && my_divisor != 0), NULL);
    }
    adaptive_partition_type_base(adaptive_partition_type_base &src, const proportional_split& split_obj) {
        my_max_depth = src.my_max_depth;
        my_divisor = size_t(float(src.my_divisor) * float(split_obj.right())
                            / float(split_obj.left() + split_obj.right()));
        src.my_divisor -= my_divisor;
    }
    bool check_being_stolen( task &t) { // part of old should_execute_range()
        if( !my_divisor ) { // if not from the top P tasks of binary tree
            my_divisor = 1; // TODO: replace by on-stack flag (partition_state's member)?
            if( t.is_stolen_task() && t.parent()->ref_count() >= 2 ) { // runs concurrently with the left task
#if TBB_USE_EXCEPTIONS
                // RTTI is available, check whether the cast is valid
                __TBB_ASSERT(dynamic_cast<flag_task*>(t.parent()), 0);
                // correctness of the cast relies on avoiding the root task for which:
                // - initial value of my_divisor != 0 (protected by separate assertion)
                // - is_stolen_task() always returns false for the root task.
#endif
                flag_task::mark_task_stolen(t);
                if( !my_max_depth ) my_max_depth++;
                my_max_depth += __TBB_DEMAND_DEPTH_ADD;
                return true;
            }
        }
        return false;
    }
    void align_depth(depth_t base) {
        __TBB_ASSERT(base <= my_max_depth, 0);
        my_max_depth -= base;
    }
    depth_t max_depth() { return my_max_depth; }
};

//! Helper that enables one or the other code branches (see example in is_range_divisible_in_proportion)
template<bool C, typename T = void> struct enable_if { typedef T type; };
template<typename T> struct enable_if<false, T> { };

//! Class determines whether template parameter has static boolean
//! constant 'is_divisible_in_proportion' initialized with value of
//! 'true' or not.
/** If template parameter has such field that has been initialized
 *  with non-zero value then class field will be set to 'true',
 *  otherwise - 'false'
 */
template <typename Range>
class is_range_divisible_in_proportion {
private:
    typedef char yes[1];
    typedef char no [2];

    template <typename range_type> static yes& decide(typename enable_if<range_type::is_divisible_in_proportion>::type *);
    template <typename range_type> static no& decide(...);
public:
    // equals to 'true' if and only if static const variable 'is_divisible_in_proportion' of template parameter
    // initialized with the value of 'true'
    static const bool value = (sizeof(decide<Range>(0)) == sizeof(yes));
};

//! Provides default methods for affinity (adaptive) partition objects.
class affinity_partition_type : public adaptive_partition_type_base<affinity_partition_type> {
    static const unsigned factor_power = 4;
    static const unsigned factor = 1<<factor_power;  // number of slots in affinity array per task
    enum {
        start = 0,
        run,
        pass
    } my_delay;
#ifdef __TBB_USE_MACHINE_TIME_STAMPS
    machine_tsc_t my_dst_tsc;
#endif
    size_t my_begin;
    tbb::internal::affinity_id* my_array;
public:
    typedef proportional_split split_type;

    affinity_partition_type( tbb::internal::affinity_partitioner_base_v3& ap )
        : adaptive_partition_type_base<affinity_partition_type>(),
          my_delay(start)
#ifdef __TBB_USE_MACHINE_TIME_STAMPS
        , my_dst_tsc(0)
#endif
        {
        __TBB_ASSERT( (factor&(factor-1))==0, "factor must be power of two" );
        my_divisor *= factor;
        ap.resize(factor);
        my_array = ap.my_array;
        my_begin = 0;
        my_max_depth = factor_power + 1; // the first factor_power ranges will be spawned, and >=1 ranges should be left
        __TBB_ASSERT( my_max_depth < __TBB_RANGE_POOL_CAPACITY, 0 );
    }
    affinity_partition_type(affinity_partition_type& p, split)
        : adaptive_partition_type_base<affinity_partition_type>(p, split()),
          my_delay(pass),
#ifdef __TBB_USE_MACHINE_TIME_STAMPS
          my_dst_tsc(0),
#endif
          my_array(p.my_array) {
        // the sum of the divisors represents original value of p.my_divisor before split
        __TBB_ASSERT(my_divisor + p.my_divisor <= factor, NULL);
        my_begin = p.my_begin + p.my_divisor;
    }
    affinity_partition_type(affinity_partition_type& p, const proportional_split& split_obj)
        : adaptive_partition_type_base<affinity_partition_type>(p, split_obj),
          my_delay(start),
#ifdef __TBB_USE_MACHINE_TIME_STAMPS
          my_dst_tsc(0),
#endif
          my_array(p.my_array) {
        size_t total_divisor = my_divisor + p.my_divisor;
        __TBB_ASSERT(total_divisor % factor == 0, NULL);
        my_divisor = (my_divisor + factor/2) & (0u - factor);
        if (!my_divisor)
            my_divisor = factor;
        else if (my_divisor == total_divisor)
            my_divisor = total_divisor - factor;
        p.my_divisor = total_divisor - my_divisor;
        __TBB_ASSERT(my_divisor && p.my_divisor, NULL);
        my_begin = p.my_begin + p.my_divisor;
    }
    void set_affinity( task &t ) {
        if( my_divisor ) {
            if( !my_array[my_begin] ) {
                // TODO: consider code reuse for static_paritioner
                my_array[my_begin] = affinity_id(my_begin / factor + 1);
            }
            t.set_affinity( my_array[my_begin] );
        }
    }
    void note_affinity( task::affinity_id id ) {
        if( my_divisor )
            my_array[my_begin] = id;
    }
    bool check_for_demand( task &t ) {
        if( pass == my_delay ) {
            if( my_divisor > 1 ) // produce affinitized tasks while they have slot in array
                return true; // do not do my_max_depth++ here, but be sure range_pool is splittable once more
            else if( my_divisor && my_max_depth ) { // make balancing task
                my_divisor = 0; // once for each task; depth will be decreased in align_depth()
                return true;
            }
            else if( flag_task::is_peer_stolen(t) ) {
                my_max_depth += __TBB_DEMAND_DEPTH_ADD;
                return true;
            }
        } else if( start == my_delay ) {
#ifndef __TBB_USE_MACHINE_TIME_STAMPS
            my_delay = pass;
#else
            my_dst_tsc = __TBB_machine_time_stamp() + __TBB_task_duration();
            my_delay = run;
        } else if( run == my_delay ) {
            if( __TBB_machine_time_stamp() < my_dst_tsc ) {
                __TBB_ASSERT(my_max_depth > 0, NULL);
                return false;
            }
            my_delay = pass;
            return true;
#endif // __TBB_USE_MACHINE_TIME_STAMPS
        }
        return false;
    }
    bool is_divisible() { // part of old should_execute_range()
        return my_divisor > factor;
    }

#if _MSC_VER && !defined(__INTEL_COMPILER)
    // Suppress "conditional expression is constant" warning.
    #pragma warning( push )
    #pragma warning( disable: 4127 )
#endif
    template <typename Range>
    split_type get_split() {
        if (is_range_divisible_in_proportion<Range>::value) {
            size_t size = my_divisor / factor;
#if __TBB_NONUNIFORM_TASK_CREATION
            size_t right = (size + 2) / 3;
#else
            size_t right = size / 2;
#endif
            size_t left = size - right;
            return split_type(left, right);
        } else {
            return split_type(1, 1);
        }
    }
#if _MSC_VER && !defined(__INTEL_COMPILER)
    #pragma warning( pop )
#endif // warning 4127 is back

    static const unsigned range_pool_size = __TBB_RANGE_POOL_CAPACITY;
};

class auto_partition_type: public adaptive_partition_type_base<auto_partition_type> {
public:
    auto_partition_type( const auto_partitioner& ) {
        my_divisor *= __TBB_INITIAL_CHUNKS;
    }
    auto_partition_type( auto_partition_type& src, split)
      : adaptive_partition_type_base<auto_partition_type>(src, split()) {}

    bool is_divisible() { // part of old should_execute_range()
        if( my_divisor > 1 ) return true;
        if( my_divisor && my_max_depth ) { // can split the task. TODO: on-stack flag instead
            // keep same fragmentation while splitting for the local task pool
            my_max_depth--;
            my_divisor = 0; // decrease max_depth once per task
            return true;
        } else return false;
    }
    bool check_for_demand(task &t) {
        if( flag_task::is_peer_stolen(t) ) {
            my_max_depth += __TBB_DEMAND_DEPTH_ADD;
            return true;
        } else return false;
    }

    static const unsigned range_pool_size = __TBB_RANGE_POOL_CAPACITY;
};

class simple_partition_type: public partition_type_base<simple_partition_type> {
public:
    simple_partition_type( const simple_partitioner& ) {}
    simple_partition_type( const simple_partition_type&, split ) {}
    //! simplified algorithm
    template<typename StartType, typename Range>
    void execute(StartType &start, Range &range) {
        split_type split_obj = split(); // start.offer_work accepts split_type as reference
        while( range.is_divisible() )
            start.offer_work( split_obj );
        start.run_body( range );
    }
    //static const unsigned range_pool_size = 1; - not necessary because execute() is overridden
};

//! Backward-compatible partition for auto and affinity partition objects.
class old_auto_partition_type: public tbb::internal::partition_type_base {
    size_t num_chunks;
    static const size_t VICTIM_CHUNKS = 4;
public:
    bool should_execute_range(const task &t) {
        if( num_chunks<VICTIM_CHUNKS && t.is_stolen_task() )
            num_chunks = VICTIM_CHUNKS;
        return num_chunks==1;
    }
    old_auto_partition_type( const auto_partitioner& )
      : num_chunks(internal::get_initial_auto_partitioner_divisor()*__TBB_INITIAL_CHUNKS/4) {}
    old_auto_partition_type( const affinity_partitioner& )
      : num_chunks(internal::get_initial_auto_partitioner_divisor()*__TBB_INITIAL_CHUNKS/4) {}
    old_auto_partition_type( old_auto_partition_type& pt, split ) {
        num_chunks = pt.num_chunks = (pt.num_chunks+1u) / 2u;
    }
};

} // namespace interfaceX::internal
//! @endcond
} // namespace interfaceX

//! A simple partitioner
/** Divides the range until the range is not divisible.
    @ingroup algorithms */
class simple_partitioner {
public:
    simple_partitioner() {}
private:
    template<typename Range, typename Body, typename Partitioner> friend class serial::interface7::start_for;
    template<typename Range, typename Body, typename Partitioner> friend class interface7::internal::start_for;
    template<typename Range, typename Body, typename Partitioner> friend class interface7::internal::start_reduce;
    template<typename Range, typename Body, typename Partitioner> friend class internal::start_scan;
    // backward compatibility
    class partition_type: public internal::partition_type_base {
    public:
        bool should_execute_range(const task& ) {return false;}
        partition_type( const simple_partitioner& ) {}
        partition_type( const partition_type&, split ) {}
    };
    // new implementation just extends existing interface
    typedef interface7::internal::simple_partition_type task_partition_type;

    // TODO: consider to make split_type public
    typedef interface7::internal::simple_partition_type::split_type split_type;
};

//! An auto partitioner
/** The range is initial divided into several large chunks.
    Chunks are further subdivided into smaller pieces if demand detected and they are divisible.
    @ingroup algorithms */
class auto_partitioner {
public:
    auto_partitioner() {}

private:
    template<typename Range, typename Body, typename Partitioner> friend class serial::interface7::start_for;
    template<typename Range, typename Body, typename Partitioner> friend class interface7::internal::start_for;
    template<typename Range, typename Body, typename Partitioner> friend class interface7::internal::start_reduce;
    template<typename Range, typename Body, typename Partitioner> friend class internal::start_scan;
    // backward compatibility
    typedef interface7::internal::old_auto_partition_type partition_type;
    // new implementation just extends existing interface
    typedef interface7::internal::auto_partition_type task_partition_type;

    // TODO: consider to make split_type public
    typedef interface7::internal::auto_partition_type::split_type split_type;
};

//! An affinity partitioner
class affinity_partitioner: internal::affinity_partitioner_base_v3 {
public:
    affinity_partitioner() {}

private:
    template<typename Range, typename Body, typename Partitioner> friend class serial::interface7::start_for;
    template<typename Range, typename Body, typename Partitioner> friend class interface7::internal::start_for;
    template<typename Range, typename Body, typename Partitioner> friend class interface7::internal::start_reduce;
    template<typename Range, typename Body, typename Partitioner> friend class internal::start_scan;
    // backward compatibility - for parallel_scan only
    typedef interface7::internal::old_auto_partition_type partition_type;
    // new implementation just extends existing interface
    typedef interface7::internal::affinity_partition_type task_partition_type;

    // TODO: consider to make split_type public
    typedef interface7::internal::affinity_partition_type::split_type split_type;
};

} // namespace tbb

#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
    #pragma warning (pop)
#endif // warning 4244 is back
#undef __TBB_INITIAL_CHUNKS
#undef __TBB_RANGE_POOL_CAPACITY
#undef __TBB_INIT_DEPTH
#endif /* __TBB_partitioner_H */
