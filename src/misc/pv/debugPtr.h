/*
 * Copyright information and license terms for this software can be
 * found in the file LICENSE that is included with the distribution
 */
/* Author:  Michael Davidsaver */
/* wrapper around shared_ptr which tracks backwards references.
 * Can help to find ref. leaks, loops, and other exciting bugs.
 * See comments in sharedPtr.h
 */
#ifndef DEBUGPTR_H
#define DEBUGPTR_H

#if __cplusplus<201103L
#  error c++11 required
#endif

#include <ostream>
#include <memory>
#include <set>

#include <pv/epicsException.h>

#include <shareLib.h>

//! User code should test this macro
//! before calling epics::debug::shared_ptr::show_referrers()
#define HAVE_SHOW_REFS

namespace epics {
namespace debug {

struct tracker;
class shared_ptr_base;

class epicsShareClass ptr_base {
    friend class shared_ptr_base;
    template<typename A>
    friend class shared_ptr;
    template<typename A>
    friend class weak_ptr;
protected:
    typedef std::shared_ptr<tracker> track_t;
    track_t track;

    // shadow copy of "real" pointer in sub-class
    // avoids the need for virtual methods in templated sub-class
    // (a recipe for DLL import/export problems on windows)
    const char *base;
    size_t bsize;

    ptr_base() noexcept :base(0), bsize(0) {}

    ptr_base(const track_t& t, const void *base, size_t bsize)
        :track(t), base((const char*)base), bsize(bsize)
    {}

    ptr_base(const void* base, size_t bsize)
        :base((const char*)base), bsize(bsize)
    {}

    ptr_base(const ptr_base& o)
        :track(o.track), base(o.base), bsize(o.bsize)
    {}
    ptr_base(ptr_base&& o)
        :track(std::move(o.track)), base(o.base), bsize(o.bsize)
    {
        o.base = 0;
        o.bsize = 0;
    }

    ptr_base& operator=(const ptr_base& o) = delete;
    ptr_base& operator=(ptr_base&& o) = delete;

    void base_assign(const ptr_base& o)
    {
        if(this!=&o) {
            track = o.track;
            base = o.base;
            bsize = o.bsize;
        }
    }

    void base_move(ptr_base&& o)
    {
        if(this!=&o) {
            track = std::move(o.track);
            base = o.base;
            o.base = 0;
            bsize = o.bsize;
            o.bsize = 0;
        }
    }

    void swap(ptr_base& o)
    {
        if(this!=&o) {
            std::swap(track, o.track);
            std::swap(base, o.base);
            std::swap(bsize, o.bsize);
        }
    }

public:
    // show refs which are referred to by our object.
    // aka shared_ptr contained within our object
    void show_referents(std::ostream& strm) const;

    // show refs which refer to our object (possibly including me)
    void show_referrers(std::ostream& strm, bool self=true) const;

    // see if we refer to the given pointer, either directly or indirectly
    bool refers_to(const void* ptr) const;

    // are we part of a detectable ref. loop
    inline bool refers_self() const { return refers_to(base); }

    typedef std::set<const shared_ptr_base *> ref_set_t;
    void spy_refs(ref_set_t&) const;
};

class epicsShareClass weak_ptr_base : public ptr_base {
protected:
    weak_ptr_base() {}
    weak_ptr_base(const weak_ptr_base& o) :ptr_base(o) {}
    weak_ptr_base(const shared_ptr_base& o);
};

class epicsShareClass shared_ptr_base : public ptr_base {
protected:
    shared_ptr_base() noexcept
        :ptr_base()
        ,m_stack(), m_depth(0)
    {}

    // begin tracking new pointer
    shared_ptr_base(const void *base, size_t bsize);

    shared_ptr_base(const track_t& t, const void *base, size_t bsize);

    shared_ptr_base(const shared_ptr_base& other);
    shared_ptr_base(const weak_ptr_base& other);

    shared_ptr_base(shared_ptr_base&& other);

    ~shared_ptr_base();

    void dotrack();
    void dountrack();

    void base_assign(const ptr_base& o);
    void base_move(shared_ptr_base&& o);

    void swap(shared_ptr_base &o);

    void reset(const void *ptr, size_t ps);

    void snap_stack();

    void *m_stack[EXCEPT_DEPTH];
    int m_depth; // always <= EXCEPT_DEPTH

public:
    void show_stack(std::ostream&) const;
};


inline weak_ptr_base::weak_ptr_base(const shared_ptr_base& o) :ptr_base(o) {}

template<typename T>
class shared_ptr;
template<typename T>
class weak_ptr;
template<class Base>
class enable_shared_from_this;

template<typename Store, typename Actual>
inline void
do_enable_shared_from_this(const shared_ptr<Store>& dest,
                            enable_shared_from_this<Actual>* self
                            );

template<typename T>
inline void
do_enable_shared_from_this(const shared_ptr<T>&, ...) {}

template<typename T>
class shared_ptr : public shared_ptr_base {
    typedef ::std::shared_ptr<T> real_type;

    real_type real;

    template<typename A>
    friend class shared_ptr;
    template<typename A>
    friend class weak_ptr;

    // ctor for casts
    shared_ptr(const real_type& r, const ptr_base::track_t& t)
        :shared_ptr_base(t, r ? r.get() : 0, sizeof(real_type)), real(r)
    {}
public:
    typedef typename real_type::element_type element_type;
    typedef weak_ptr<T> weak_type;

    // new NULL
    shared_ptr() noexcept :shared_ptr_base(0,0) {}
    // copy existing same type
    shared_ptr(const shared_ptr& o) :shared_ptr_base(o), real(o.real) {}
    // copy existing of implicitly castable type
    template<typename A>
    shared_ptr(const shared_ptr<A>& o) :shared_ptr_base(o), real(o.real) {}

    // construct around new pointer
    template<typename A, class ... Args>
    explicit shared_ptr(A* a, Args ... args) : shared_ptr_base(a, sizeof(A)), real(a, args...) {
        do_enable_shared_from_this(*this, a);
    }

    // make strong ref from weak
    template<typename A>
    shared_ptr(const weak_ptr<A>& o)
        :shared_ptr_base(o), real(o.real)
    {}

    // takeover from unique_ptr
    template<typename A>
    shared_ptr(std::unique_ptr<A>&& a) : shared_ptr_base(a.get(), sizeof(A)), real(a.release()) {}

    ~shared_ptr() {}

    shared_ptr& operator=(const shared_ptr& o) {
        if(this!=&o) {
            base_assign(o);
            real = o.real;
        }
        return *this;
    }
    template<typename A>
    shared_ptr& operator=(const shared_ptr<A>& o) {
        if(get()!=o.get()) {
            base_assign(o);
            real = o.real;
        }
        return *this;
    }

    void reset() noexcept { real.reset(); shared_ptr_base::reset(0,0); }
    template<typename A, class ... Args>
    void reset(A* a, Args ... args)
    {
        real.reset(a, args...);
        shared_ptr_base::reset(a, sizeof(A));
        do_enable_shared_from_this(*this, a);
    }
    void swap(shared_ptr &o) noexcept
    {
        if(this!=&o) {
            shared_ptr_base::swap(o);
            real.swap(o.real);
        }
    }

    // proxy remaining to underlying shared_ptr

    T* get() const noexcept { return real.get(); }
    typename std::add_lvalue_reference<T>::type operator*() const noexcept { return *real; }
    T* operator->() const noexcept { return real.get(); }
    long use_count() const noexcept { return real.use_count(); }
    bool unique() const noexcept { return real.unique(); }
    explicit operator bool() const noexcept { return bool(real); }

    bool operator==(const shared_ptr<T>& o) const { return real==o.real; }
    bool operator!=(const shared_ptr<T>& o) const { return real!=o.real; }
    bool operator<(const shared_ptr<T>& o) const { return real<o.real; }

    template<typename A>
    bool owner_before(const shared_ptr<A>& o) { return real.owner_before(o); }
    template<typename A>
    bool owner_before(const weak_ptr<A>& o) { return real.owner_before(o); }

    template<typename TO, typename FROM>
    friend
    shared_ptr<TO> static_pointer_cast(const shared_ptr<FROM>& src);
    template<typename TO, typename FROM>
    friend
    shared_ptr<TO> const_pointer_cast(const shared_ptr<FROM>& src);
    template<typename TO, typename FROM>
    friend
    shared_ptr<TO> dynamic_pointer_cast(const shared_ptr<FROM>& src);
    template<typename Store, typename Actual>
    friend void
    do_enable_shared_from_this(const shared_ptr<Store>& dest,
                                enable_shared_from_this<Actual>* self
                                );
};

template<typename TO, typename FROM>
shared_ptr<TO> static_pointer_cast(const shared_ptr<FROM>& src) {
    return shared_ptr<TO>(std::static_pointer_cast<TO>(src.real), src.track);
}

template<typename TO, typename FROM>
shared_ptr<TO> const_pointer_cast(const shared_ptr<FROM>& src) {
    return shared_ptr<TO>(std::const_pointer_cast<TO>(src.real), src.track);
}

template<typename TO, typename FROM>
shared_ptr<TO> dynamic_pointer_cast(const shared_ptr<FROM>& src) {
    return shared_ptr<TO>(std::dynamic_pointer_cast<TO>(src.real), src.track);
}

template<typename T>
class weak_ptr : public weak_ptr_base {
    typedef ::std::weak_ptr<T> real_type;

    real_type real;

    template<typename A>
    friend class shared_ptr;
    template<typename A>
    friend class weak_ptr;

public:
    typedef typename real_type::element_type element_type;
    typedef weak_ptr<T> weak_type;

    // new NULL
    weak_ptr() noexcept {}
    // copy existing same type
    weak_ptr(const weak_ptr& o) :weak_ptr_base(o), real(o.real) {}
    // copy existing of similar type
    template<typename A>
    weak_ptr(const weak_ptr<A>& o) :weak_ptr_base(o), real(o.real) {}

    // create week ref from strong ref
    template<typename A>
    weak_ptr(const shared_ptr<A>& o) :weak_ptr_base(o), real(o.real) {}

    ~weak_ptr() {}

    weak_ptr& operator=(const weak_ptr& o) {
        if(this!=&o) {
            real = o.real;
            track = o.track;
        }
        return *this;
    }
    template<typename A>
    weak_ptr& operator=(const shared_ptr<A>& o) {
        real = o.real;
        track = o.track;
        return *this;
    }

    shared_ptr<T> lock() const noexcept { return shared_ptr<T>(real.lock(), track); }
    void reset() noexcept { track.reset(); real.reset(); }

    long use_count() const noexcept { return real.use_count(); }
    bool unique() const noexcept { return real.unique(); }
};

template<class Base>
class enable_shared_from_this {
    mutable weak_ptr<Base> xxInternalSelf;

    template<typename Store, typename Actual>
    friend
    void
    do_enable_shared_from_this(const shared_ptr<Store>& dest,
                                enable_shared_from_this<Actual>* self
                                );
public:
    shared_ptr<Base> shared_from_this() const {
        return shared_ptr<Base>(xxInternalSelf);
    }
};

template<typename Store, typename Actual>
inline void
do_enable_shared_from_this(const shared_ptr<Store>& dest,
                            enable_shared_from_this<Actual>* self
                            )
{
    shared_ptr<Actual> actual(dynamic_pointer_cast<Actual>(dest));
    if(!actual)
        throw std::logic_error("epics::debug::enabled_shared_from_this fails");
    self->xxInternalSelf = actual;
}

}} // namespace epics::debug

template<typename T>
inline std::ostream& operator<<(std::ostream& strm, const epics::debug::shared_ptr<T>& ptr)
{
    strm<<ptr.get();
    return strm;
}

#endif // DEBUGPTR_H
