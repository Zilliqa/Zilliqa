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

#ifndef __TBB_parallel_for_each_H
#define __TBB_parallel_for_each_H

#include "parallel_do.h"

namespace tbb {

//! @cond INTERNAL
namespace internal {
    // The class calls user function in operator()
    template <typename Function, typename Iterator>
    class parallel_for_each_body : internal::no_assign {
        const Function &my_func;
    public:
        parallel_for_each_body(const Function &_func) : my_func(_func) {}
        parallel_for_each_body(const parallel_for_each_body<Function, Iterator> &_caller) : my_func(_caller.my_func) {}

        void operator() ( typename std::iterator_traits<Iterator>::reference value ) const {
            my_func(value);
        }
    };
} // namespace internal
//! @endcond

/** \name parallel_for_each
    **/
//@{
//! Calls function f for all items from [first, last) interval using user-supplied context
/** @ingroup algorithms */
#if __TBB_TASK_GROUP_CONTEXT
template<typename InputIterator, typename Function>
void parallel_for_each(InputIterator first, InputIterator last, const Function& f, task_group_context &context) {
    internal::parallel_for_each_body<Function, InputIterator> body(f);
    tbb::parallel_do (first, last, body, context);
}

//! Calls function f for all items from rng using user-supplied context
/** @ingroup algorithms */
template<typename Range, typename Function>
void parallel_for_each(Range& rng, const Function& f, task_group_context& context) {
    parallel_for_each(tbb::internal::first(rng), tbb::internal::last(rng), f, context);
}

//! Calls function f for all items from const rng user-supplied context
/** @ingroup algorithms */
template<typename Range, typename Function>
void parallel_for_each(const Range& rng, const Function& f, task_group_context& context) {
    parallel_for_each(tbb::internal::first(rng), tbb::internal::last(rng), f, context);
}
#endif /* __TBB_TASK_GROUP_CONTEXT */

//! Uses default context
template<typename InputIterator, typename Function>
void parallel_for_each(InputIterator first, InputIterator last, const Function& f) {
    internal::parallel_for_each_body<Function, InputIterator> body(f);
    tbb::parallel_do (first, last, body);
}

//! Uses default context
template<typename Range, typename Function>
void parallel_for_each(Range& rng, const Function& f) {
    parallel_for_each(tbb::internal::first(rng), tbb::internal::last(rng), f);
}

//! Uses default context
template<typename Range, typename Function>
void parallel_for_each(const Range& rng, const Function& f) {
    parallel_for_each(tbb::internal::first(rng), tbb::internal::last(rng), f);
}

//@}

} // namespace

#endif /* __TBB_parallel_for_each_H */
