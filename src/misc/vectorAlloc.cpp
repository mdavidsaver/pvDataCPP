/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * EPICS pvData is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */
/* Author:  Michael Davidsaver */
#include <stdexcept>
#include <vector>

#include <epicsMutex.h>
#include <epicsThread.h>

#include "vectorAlloc.h"

static epicsMutexId poolListLock;
static epicsThreadOnceId poolListOnce = EPICS_THREAD_ONCE_INIT;
static ELLLIST poolList;

static void initPoolList(void*);
#define doInitPoolList() epicsThreadOnce(&poolListOnce, &initPoolList, NULL)

static void showStats(void *raw, const ::epics::pvData::allocator_info& info)
{
    std::ostream *strm=(std::ostream*)raw;
    (*strm)<<info;
}

namespace epics { namespace pvData {

allocator_info::allocator_info()
    :name()
    ,num_allocs(0)
    ,size_allocs(0)
    ,num_free(0)
    ,size_free(0)
    ,allocsize(0)
    ,fixedsize(false)
    ,has_stats(false)
{}

std::ostream& operator<<(std::ostream& strm, const allocator_info& info)
{
    strm<<"Name: ";
    if(info.name.empty())
        strm<<"<unnamed>\n";
    else
        strm<<info.name<<"\n";
    if(info.fixedsize)
        strm<<" Size: " <<info.allocsize<<"\n";
    else
        strm<<" Size: dynamic\n";
    if(info.has_stats)
        strm<<" Alloc: "<<info.num_allocs <<" "<<info.size_allocs<<"\n"
            <<" Free : "<<info.num_free <<" "<<info.size_free<<"\n";
    return strm;
}

void collect_allocator_info(void* priv,
                            void (*cb)(void*,
                                       const allocator_info&))
{

    epicsMutexMustLock(poolListLock);
    size_t nallocs = (size_t)ellCount(&poolList);
    epicsMutexUnlock(poolListLock);

    typedef std::vector<allocator_info> infos_t;
    infos_t infos;
    // Just reserve as this number might change...
    infos.reserve(nallocs);
    
    epicsMutexMustLock(poolListLock);
    for(ELLNODE *cur = ellFirst(&poolList);
        cur;
        cur = ellNext(cur))
    {
        detail::vector_allocator_impl::node_t* ptr= (detail::vector_allocator_impl::node_t*)cur;
        allocator_info I;
        if(ptr->self) {
            try{
                ptr->self->collect_info(I);
                infos.push_back(I);
            }catch(...) {
                //TODO: what to do here?
            }
        }
    }
    epicsMutexUnlock(poolListLock);
    
    for(infos_t::const_iterator it=infos.begin(), end=infos.end();
        it!=end; ++it)
    {
        (*cb)(priv, *it);
    }
}

void print_allocator_info(std::ostream &strm)
{
    strm<<"# Allocator info\n";
    collect_allocator_info((void*)&strm, &showStats);
    strm<<"# End Allocator info\n";
}

namespace detail {
vector_allocator_impl::vector_allocator_impl(const std::string& n)
    :name(n)
{
    doInitPoolList();
    epicsMutexMustLock(poolListLock);
    node.self = this;
    ellAdd(&poolList, &node.node);
    epicsMutexUnlock(poolListLock);
}

vector_allocator_impl::~vector_allocator_impl()
{
    assert(node.self==this);
    epicsMutexMustLock(poolListLock);
    ellDelete(&poolList, &node.node);
    epicsMutexUnlock(poolListLock);
    node.self=NULL;
}

void
vector_allocator_impl::alloc(shared_vector<void>& ret,
                             size_t e, size_t n, bool zero)
{
    throw std::logic_error("vector_allocator doesn't allocate?");
}

void
vector_allocator_impl::collect_info(allocator_info& s) const
{
    s.name = name;
}

}// namespace

}} // namespace

static void initPoolList(void*)
{
    poolListLock = epicsMutexMustCreate();
}
