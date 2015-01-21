/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * EPICS pvData is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */
/* Author:  Michael Davidsaver */
#include <stdexcept>
#include <vector>
#include <map>

#include <cantProceed.h>
#include <epicsMutex.h>
#include <epicsGuard.h>
#include <epicsThread.h>
#include <epicsStdio.h>

#include "vectorAlloc.h"

using namespace epics::pvData;

static epicsThreadOnceId poolSingletonOnce = EPICS_THREAD_ONCE_INIT;

static void initPoolSingleton(void*);
#define doInitPoolSingleton() epicsThreadOnce(&poolSingletonOnce, &initPoolSingleton, NULL)

namespace {

typedef epicsGuard<epicsMutex> mutexGuard;
typedef epicsGuardRelease<epicsMutex> mutexUnguard;
class default_allocator;

struct free_deleter {void operator()(void* a){free(a);}};

//! Default dynamic allocator using calloc/malloc and free
//! A singleton
class default_allocator : public detail::vector_allocator_impl
{
public:
    default_allocator() : detail::vector_allocator_impl("Default Allocator") {}
    virtual ~default_allocator() {}


    virtual void alloc(shared_vector<void>& ret,
                       size_t e, size_t n, bool zero)
    {
        void *A;
        if(zero)
            A = calloc(e, n);
        else
            A = malloc(e*n);
        if(!A)
            throw std::bad_alloc();
        shared_vector<void> V(A, free_deleter(), 0, e*n);
        ret.swap(V);
    }

    virtual void collect_info(allocator_info& s) const
    {
        s.name = name;
        s.num_allocs = s.size_allocs = 0;
        s.num_free = s.size_free = 0;
        s.allocsize = 0;
        s.fixedsize = false;
        s.has_stats = false;
    }
    
    static std::tr1::shared_ptr<default_allocator> *singleton;
};

std::tr1::shared_ptr<default_allocator> *default_allocator::singleton;

class freelist_allocator;

struct freelist_deleter
{
    typedef std::tr1::shared_ptr<freelist_allocator> alloc_t;
    alloc_t alloc;
    freelist_deleter(const alloc_t& a) :alloc(a) {}
    void operator()(void* a);
};

/** Free list of fixed length buffers.
 *
 *  Requests for lengths greater than allocsize will throw std::bad_alloc.
 *  Requests for less than or equal to allocsize will be satisfied with allocsize
 *
 *  Two modes.
 *
 *  1. capped=true.  While nallocd==numalloc, additional requests will throw
 *    std::bad_alloc.
 *  2. capped=false.  While nallocd>=numalloc, additional requests will
 *    be allocated, and reclaimed buffers will be immediately free'd.
 */
class freelist_allocator : public detail::vector_allocator_impl,
                           public std::tr1::enable_shared_from_this<freelist_allocator>
{
public:
    const size_t elemsize, allocsize, numalloc;
    const bool capped;
    mutable epicsMutex lock;
    std::vector<void*> flist;
    size_t nallocd;
    
    freelist_allocator(const std::string& name, size_t e, size_t a, size_t n, size_t i, bool c)
        :detail::vector_allocator_impl(name)
        ,elemsize(e)
        ,allocsize(a)
        ,numalloc(n)
        ,capped(c)
        ,lock()
        ,flist()
        ,nallocd(0)
    {
        assert(i<=numalloc);
        flist.reserve(n);
        for(;i;--i) {
            void *A = malloc(elemsize*allocsize);
            if(!A)
                throw std::bad_alloc();
            flist.push_back(A);
        }
    }
    virtual ~freelist_allocator()
    {
        assert(nallocd==0);
        mutexGuard g(lock); // paranoia
        while(!flist.empty()) {
            free(flist.back());
            flist.pop_back();
        }
    }

    virtual void alloc(shared_vector<void>& ret,
                       size_t e, size_t n, bool zero)
    {
        mutexGuard g(lock);
        size_t asize = e*n;
        void *A;

        assert(!capped || nallocd<=numalloc);
        assert(!capped || nallocd+flist.size()<=numalloc);
        
        if(asize>elemsize*allocsize || (capped && nallocd==numalloc))
            throw std::bad_alloc();

        else if(!flist.empty()) {
            A = flist.back();
            flist.pop_back();
            nallocd++;
            if(zero)
                memset(A, 0, elemsize*allocsize);

        } else {
            nallocd++;
            {
                mutexUnguard u(g);
                if(zero)
                    A = calloc(elemsize, allocsize);
                else
                    A = malloc(elemsize*allocsize);
            }
            if(!A) {
                nallocd--;
                throw std::bad_alloc();
            }
        }
        shared_vector<void> V(A, freelist_deleter(shared_from_this()), 0, asize);
        ret.swap(V);
    }

    virtual void collect_info(allocator_info& s) const
    {
        mutexGuard g(lock);
        s.name = name;

        s.fixedsize = true;
        s.allocsize = elemsize*allocsize;

        s.has_stats = true;
        s.num_allocs = nallocd;
        s.size_allocs = nallocd*elemsize*allocsize;
        s.num_free = flist.size();
        s.size_free = flist.size()*elemsize*allocsize;
    }
};

void freelist_deleter::operator ()(void *a)
{
#if !defined(NDEBUG) && !defined(NOMAGIC)
    memset(a, 0x1b, alloc->elemsize*alloc->allocsize);
#endif
    {
        mutexGuard g(alloc->lock);
        assert(alloc->nallocd>0);
        alloc->nallocd--;
        // return to the freelist if there is room
        if(alloc->flist.size()<alloc->numalloc) {
            alloc->flist.push_back(a);
            return;
        }
    }
    free(a);
}

}//namespace

namespace epics { namespace pvData {
pool_builder::pool_builder()
    :is_fixed(false)
    ,asize(0)
    ,psize(0)
    ,ipsize(1)
    ,sname()
{}

pool_builder::~pool_builder() {}

pool_builder& pool_builder::name(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    nameV(fmt, args);
    va_end(args);
    return *this;
}

pool_builder& pool_builder::nameV(const char *fmt, va_list args)
{
    char buf[60];
    epicsVsnprintf(buf, sizeof(buf), fmt, args);
    buf[sizeof(buf)-1] = '\0';
    sname = buf;
    return *this;
}

detail::vector_allocator_impl::impl_t
pool_builder::build_impl(size_t e)
{
    assert(e>0);

    // by default we just build the default...
    if(!is_fixed) {
        // ignore can_share (always shared)
        // ignore psize hint
        // ignore name (if not requested shared)
        doInitPoolSingleton();
        return *default_allocator::singleton;
    }

    if(asize==0)
        throw std::invalid_argument("fixed() allocation size must be >0");

    //TODO: implement sharing
    return detail::vector_allocator_impl::impl_t(new freelist_allocator(sname, e, asize, psize,
                                                                        ipsize, is_bounded));
}

}}//namespace

static void initPoolSingleton(void*)
{
    try{
        default_allocator::singleton = new std::tr1::shared_ptr<default_allocator>(new default_allocator());
    }catch(std::exception& e){
        cantProceed("Failed to initialize initPoolSingleton(): %s", e.what());
    }
}
