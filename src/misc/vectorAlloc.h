/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * EPICS pvData is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */
/* Author:  Michael Davidsaver */
#ifndef VECTORALLOC_H
#define VECTORALLOC_H

#include <stdarg.h>
#include <ostream>

#include <compilerDependencies.h>
#include <ellLib.h>
#include "sharedVector.h"

#include <shareLib.h>

namespace epics { namespace pvData {

struct epicsShareClass allocator_info {
    allocator_info();
    std::string name;

    size_t num_allocs; //!< # of allocations outstanding
    size_t size_allocs;//!< size in bytes of allocations
    size_t num_free;   //!< # of entries in free-list
    size_t size_free;  //!< size of free-list

    //! If fixedsize==true, the allocation size.
    size_t allocsize;
    bool fixedsize; //!< Does this allocator make fixed size allocations
    //! Does this allocator keep stats.
    //! If false then the value of num_* and size_* is undefined.
    bool has_stats;
};

std::ostream& operator<<(std::ostream&, const allocator_info&);

//! The provided callback is invoked once for each active allocator.
epicsShareExtern
void collect_allocator_info(void*, void (*cb)(void*,const allocator_info&));
epicsShareExtern
void print_allocator_info(std::ostream&);

namespace detail{
/** @brief allocator implementation
 *
 * Users of vector_allocator<> should ignore this class.
 * It is only of interested to those implementing custom
 * allocators.
 */
class epicsShareClass vector_allocator_impl
{
public:
    typedef std::tr1::shared_ptr<detail::vector_allocator_impl> impl_t;

    vector_allocator_impl(const std::string& name);

    virtual ~vector_allocator_impl();

    /**
     * @brief An allocation is requested
     * @param ret The provided allocation
     * @param e element size
     * @param n number of elements
     * @param zero Should the returned memory be zero'd?
     * @throws std::bad_alloc on failure.
     *
     * This method should never return without setting ret.
     * Errors should be reported by throwing an exception.
     */
    virtual void alloc(shared_vector<void>& ret,
                       size_t e, size_t n, bool zero)=0;

    /** @brief allocator information and statistics
     *
     * At minimum an implementation must fill in:
     * name, fixedsize, and allocsize
     */
    virtual void collect_info(allocator_info& s) const=0;

    //! Every allocator has a name.
    //! Possibly not unique.
    const std::string name;
    
    struct node_t {
        ELLNODE node;
        vector_allocator_impl *self;
    } node;
};
}//namespace detail

/** @brief Allocator of shared_vector<E>
 *
 * A vector_allocator<E> sits in front of a memory pool.
 * The shared_vector<E>s returned by malloc() and calloc()
 * have size() equal to the requested number of elements.
 */
template<typename E>
class vector_allocator
{
public:
    typedef std::tr1::shared_ptr<detail::vector_allocator_impl> impl_t;

    explicit vector_allocator(const impl_t& i) :impl(i) {}
    vector_allocator(const vector_allocator& o) :impl(o.impl) {}
    ~vector_allocator() {}
    vector_allocator& operator=(const vector_allocator& o)
    { impl = o.impl; return *this; }

    //! Request uninitialized memory
    shared_vector<E> malloc(size_t n)
    {
        shared_vector<void> ret;
        impl->alloc(ret, sizeof(E), n, false);
        return static_shared_vector_cast<E>(ret);
    }
    //! Request zero'd memory
    shared_vector<E> calloc(size_t n)
    {
        shared_vector<void> ret;
        impl->alloc(ret, sizeof(E), n, true);
        return static_shared_vector_cast<E>(ret);
    }

    inline const std::string& name() const {return impl->name;}

    //! Fetch information and statistics about this allocator.
    inline allocator_info info() const
    {
        allocator_info ret;
        impl->collect_info(ret);
        return ret;
    }

    bool operator==(const vector_allocator& o)
    { return impl.get()==o.impl.get(); }
    bool operator!=(const vector_allocator& o)
    { return !(*this==o); }
    bool operator<(const vector_allocator& o)
    { return impl.get()<o.impl.get(); }

private:
    impl_t impl;
};

/** @brief Create/Fetch a shared_vector allocation pool
 *
 * This class functions as a single function with named
 * arguments.  An instance should be used only once.
 * After calling build<>() the state of the pool_builder
 * is undefined.
 *
 * To fetch the default, unbounded, shared pool.
 @code
   vector_allocator<int> alloc(pool_builder()
                               .build<int>());
 @endcode
 *
 * To create a private pool with a hard limit of five buffers with
 * 1 kilo-elements each.
 @code
   vector_allocator<int> alloc(pool_builder()
                               .name("My pool")
                               .fixed(1024)
                               .capped(5)
                               .build<int>());
   shared_vector<int> example(alloc.malloc(1024));
 @endcode
 *
 * To create a private pool with a cache of up to five buffers with
 * 1 kilo-elements each.
 @code
   vector_allocator<int> alloc(pool_builder()
                               .name("My pool %d", 2)
                               .fixed(1024)
                               .cached(5)
                               .build<int>());
   shared_vector<int> example(alloc.malloc(1024));
 @endcode
 */
class epicsShareClass pool_builder
{
public:
    pool_builder();
    virtual ~pool_builder();

    //! A name may be provided, which will appear in info dumps.
    pool_builder& name(const char *fmt, ...) EPICS_PRINTF_STYLE(2,3);
    pool_builder& nameV(const char *fmt, va_list);

    // options governing allocation sizes

    //! A pool which only allows allocations of one fixed size
    FORCE_INLINE pool_builder& fixed(size_t s)
    {is_fixed=true; asize=s; return *this;}
    //! A pool which allows allocations of any size
    FORCE_INLINE pool_builder& dynamic()
    {is_fixed=false; return *this;}

    // options governing pool size

    //! A pool which will allow n outstanding allocations.
    //! Throws std::bad_alloc for allocation n+1
    FORCE_INLINE pool_builder& capped(size_t n)
    {is_bounded=true; psize=n; return *this;}
    //! A pool with no fixed limit on the number of allocations.
    //! Hint that a cache of n allocations would be appropriate.
    FORCE_INLINE pool_builder& cached(size_t n)
    {is_bounded=false; psize=n; return *this;}
    //! Hint at initial pool size
    FORCE_INLINE pool_builder& initial(size_t i)
    {ipsize=i; return *this;}

    //! Actually build the allocator
    template<typename E>
    FORCE_INLINE vector_allocator<E> build()
    {
        return vector_allocator<E>(this->build_impl(sizeof(E)));
    }

protected:
    virtual detail::vector_allocator_impl::impl_t build_impl(size_t e);

    bool is_fixed;
    bool is_bounded;
    size_t asize, psize, ipsize;
    std::string sname;
};

}} // namespace

#endif // VECTORALLOC_H
