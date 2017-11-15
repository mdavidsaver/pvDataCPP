/*
 * Copyright information and license terms for this software can be
 * found in the file LICENSE that is included with the distribution
 */

#if __cplusplus>=201103L

#include <vector>
#include <deque>
#include <set>

#include <epicsMutex.h>
#include <epicsGuard.h>
#include <epicsThread.h>

#define epicsExportSharedSymbols
#include <pv/debugPtr.h>

namespace {
typedef epicsGuard<epicsMutex> Guard;

epicsThreadOnceId dbgGblId = EPICS_THREAD_ONCE_INIT;

struct debugGbl {
    epicsMutex mutex;
    typedef std::set<epics::debug::shared_ptr_base*> pointers_t;
    pointers_t pointers;

    void add(epics::debug::shared_ptr_base *ptr) {
        Guard G(mutex);
        pointers.insert(ptr);
    }

    void remove(epics::debug::shared_ptr_base *ptr) {
        Guard G(mutex);
        pointers.erase(ptr);
    }

    static debugGbl *singleton;

    static void once(void*) {
        singleton = new debugGbl;
    }

    static debugGbl& instance() {
        epicsThreadOnce(&dbgGblId, &once, 0);
        assert(singleton);
        return *singleton;
    }
};

debugGbl* debugGbl::singleton;

}

namespace epics {
namespace debug {

// joins together a group of ptr_base instances
// which all have the same dtor
struct tracker {
    epicsMutex mutex;
    ptr_base::ref_set_t refs;
};



shared_ptr_base::shared_ptr_base(const void *base, size_t bsize)
    :ptr_base(base, bsize)
    ,m_depth(0u)
{
    if(base) {
        track.reset(new tracker);
        dotrack();
    }
}

shared_ptr_base::shared_ptr_base(const track_t& t, const void *base, size_t bsize)
    :ptr_base(t, base, bsize)
{
    dotrack();
}

shared_ptr_base::shared_ptr_base(const shared_ptr_base& other)
    :ptr_base(other)
    ,m_depth(0u)
{
    dotrack();
}

shared_ptr_base::shared_ptr_base(const weak_ptr_base& other)
    :ptr_base(other)
    ,m_depth(0u)
{
    dotrack();
}

shared_ptr_base::shared_ptr_base(shared_ptr_base&& o)
    :ptr_base()
    ,m_depth(0u)
{
    o.dountrack();
    ptr_base::base_move(std::move(o));
    dotrack();
}

shared_ptr_base::~shared_ptr_base()
{
    dountrack();
}

void shared_ptr_base::base_assign(const ptr_base& o)
{
    dountrack();
    ptr_base::base_assign(o);
    dotrack();
}

void shared_ptr_base::base_move(shared_ptr_base&& o)
{
    dountrack();
    o.dountrack();
    ptr_base::base_move(std::move(o));
    dotrack();
}

void shared_ptr_base::swap(shared_ptr_base &o)
{
    dountrack();
    o.dountrack();
    ptr_base::swap(o);
    dotrack();
    o.dotrack();
}

void shared_ptr_base::reset(const void *ptr, size_t ps)
{
    dountrack();
    if(ptr)
        track.reset(new tracker);
    else
        track.reset();
    base = (const char*)ptr;
    bsize = ps;
    dotrack();
}

void shared_ptr_base::dotrack()
{
    if(track) {
        snap_stack();
        {
            Guard G(track->mutex);
            track->refs.insert(this);
        }
        debugGbl::instance().add(this);
    } else m_depth = 0u;
}

void shared_ptr_base::dountrack()
{
    if(track) {
        debugGbl::instance().remove(this);
        Guard G(track->mutex);
        track->refs.erase(this);
    }
}

void shared_ptr_base::snap_stack()
{
#if defined(EXCEPT_USE_BACKTRACE)
    m_depth=backtrace(m_stack,EXCEPT_DEPTH);
#else
    m_depth = 0;
#endif

}

void shared_ptr_base::show_stack(std::ostream& strm) const
{
    strm<<"ptr "<<this;
#ifndef EXCEPT_USE_NONE
    if(m_depth<=0) return;
#endif
#if defined(EXCEPT_USE_BACKTRACE)
    {

        char **symbols=backtrace_symbols(m_stack, m_depth);

        strm<<": ";
        for(int i=0; i<m_depth; i++) {
            strm<<symbols[i]<<", ";
        }

        std::free(symbols);
    }
#elif !defined(EXCEPT_USE_NONE)
    {
        strm<<": ";
        for(int i=0; i<m_depth; i++) {
            strm<<std::hex<<m_stack[i]<<" ";
        }
    }
#endif

}

void ptr_base::show_referents(std::ostream& strm) const
{
    if(!base) return;

    debugGbl& gbl = debugGbl::instance();

    Guard G(gbl.mutex);
    debugGbl::pointers_t::const_iterator it(gbl.pointers.lower_bound((shared_ptr_base*)base)), // find first >=base
                                        end(gbl.pointers.upper_bound((shared_ptr_base*)base+bsize)); // first > base+bsize
    for(; it!=end; ++it) {
        shared_ptr_base *ptr = *it;
        strm<<"# ";
        ptr->show_stack(strm);
        strm<<'\n';
    }
}

void ptr_base::show_referrers(std::ostream& strm, bool self) const
{
    if(!track) {
        strm<<"# No refs\n";
    } else {
        Guard G(track->mutex);
        for(auto ref : track->refs) {
            if(!self && ref==this) continue;
            strm<<'#';
            ref->show_stack(strm);
            strm<<'\n';
        }
    }
}

bool ptr_base::refers_to(const void* ptr) const
{
    if(!base) return false;

    std::deque<const ptr_base*> todo;
    std::set<const ptr_base*> visited;

    todo.push_back(this);

    debugGbl& gbl = debugGbl::instance();

    Guard G(gbl.mutex);

    while(!todo.empty()) {
        const ptr_base *cur = todo.front();

        todo.pop_front();

        if(visited.find(cur)!=visited.end())
            continue;

        visited.insert(cur);

        debugGbl::pointers_t::const_iterator it(gbl.pointers.lower_bound((shared_ptr_base*)cur->base)), // find first >=base
                                            end(gbl.pointers.upper_bound((shared_ptr_base*)cur->base+cur->bsize)); // first > base+bsize
        for(; it!=end; ++it) {
            shared_ptr_base *X = *it;

            if(X->base==ptr)
                return true;

            todo.push_back(X);
        }
    }

    return false;
}

void ptr_base::spy_refs(ref_set_t &refs) const
{
    if(track) {
        Guard G(track->mutex);
        refs.insert(track->refs.begin(), track->refs.end());
    }
}

}} // namespace epics::debug

#endif // __cplusplus>=201103L
