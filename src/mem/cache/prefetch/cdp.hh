/**
 * Copyright (c) 2018 Metempsy Technology Consulting
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer;
 * redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution;
 * neither the name of the copyright holders nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * Implementation of the Content-Directed Data Prefetching
 *
 * References:
 * Stateless, Content-Directed Data Prefetching Mechanism
 * Robert Cooksey, Stephan Jourdan
 */

#ifndef __MEM_CACHE_PREFETCH_CDP_HH__
#define __MEM_CACHE_PREFETCH_CDP_HH__

#define BITMASK(bits) ((1ull << (bits)) - 1)
#define BITS(x, hi, lo) (((x) >> (lo)) & BITMASK((hi) - (lo) + 1))
#include <list>
#include <map>
#include <string>
#include <vector>

#include <boost/compute/detail/lru_cache.hpp>

#include "base/sat_counter.hh"
#include "base/types.hh"
#include "debug/CDPHotVpns.hh"
#include "mem/cache/base.hh"
#include "mem/cache/prefetch/associative_set_impl.hh"
#include "mem/cache/prefetch/queued.hh"
#include "mem/packet.hh"
#include "params/CDP.hh"
#include "sim/system.hh"

#ifdef SIG_DEBUG_PRINT
#define SIG_DP(x) x
#else
#define SIG_DP(x)
#endif
namespace gem5
{

struct CDPParams;

GEM5_DEPRECATED_NAMESPACE(Prefetcher, prefetch);
namespace prefetch
{
class CDP : public Queued
{


    std::vector<bool> enable_prf_filter;
    std::vector<bool> enable_prf_filter2;
    int depth_threshold;
    int degree;
    float throttle_aggressiveness;
    bool enable_thro;
    /** Byte order used to access the cache */
    /** Update the RR right table after a prefetch fill */
    class VpnEntry : public TaggedEntry
    {
      private:
        u_int64_t refCnt;
        u_int64_t prevRefCnt;
        bool hot;
      public:
        uint64_t debug_vpn1{0};
        uint64_t debug_vpn2{0};
        VpnEntry()
            : TaggedEntry(),
              refCnt(0),
              prevRefCnt(0),
              hot(false)
        {}
        void init(uint64_t _debug_vpn1, uint64_t _debug_vpn2) {
            refCnt = 1;
            prevRefCnt = 0;
            hot = false;
            debug_vpn1 = _debug_vpn1;
            debug_vpn2 = _debug_vpn2;
        }
        void access() {
            refCnt++;
        }
        void decr() {
            prevRefCnt = prevRefCnt == 0 ? 0 : (prevRefCnt - 1);
            if (prevRefCnt == 0) {
                hot = false;
            }
        }
        void periodReset(float throttle_aggressiveness, bool enable_thro, uint64_t resetPeriod) {
            // DPRINTF(CDPHotVpns, "prevRefCnt: %d, refCnt: %d, hot: %d\n", prevRefCnt, refCnt, hot);
            if (refCnt > (resetPeriod / 16) || enable_thro) {
                prevRefCnt = 0.2 * prevRefCnt + 0.8 * (refCnt * throttle_aggressiveness);
            } else {
                prevRefCnt = 0.2 * prevRefCnt;
            }

            if (prevRefCnt > 0) {
                hot = true;
            } else {
                hot = false;
            }

            refCnt = 0;
        }
        bool getHot() { return hot; }
        u_int64_t getPrefRefCnt() { return prevRefCnt; }
        std::string name() { return std::string("VpnEntry"); }
    };

    template<class Entry>
    class VpnTable
    {
        private:
            AssociativeSet<Entry> table;
            const uint64_t _resetPeriod;
            uint64_t _resetCounter;

        public:
            VpnTable(int assoc, int num_entries, BaseIndexingPolicy *idx_policy,
            replacement_policy::Base *rpl_policy, Entry const &init_val = Entry())
                : table(assoc, num_entries, idx_policy, rpl_policy, init_val),
                  _resetPeriod(128),
                  _resetCounter(0)
            {}
            void add(int vpn2, int vpn1) {
                _resetCounter++;
                Addr cat_addr = (vpn2 << 9) | vpn1;
                Entry *entry = table.findEntry(cat_addr, true);
                if (entry) {
                    table.accessEntry(entry);
                    entry->access();
                } else {
                    entry = table.findVictim(cat_addr);
                    entry->init(vpn1, vpn2);
                    table.insertEntry(cat_addr, true, entry);
                }
            }
            void resetConfidence(float throttle_aggressiveness, bool enable_thro)
            {
                if (_resetCounter < _resetPeriod)
                    return;
                auto it = table.begin();
                while (it != table.end()) {
                    if (it->isValid()) {
                        it->periodReset(throttle_aggressiveness, enable_thro, _resetPeriod);
                    }
                    it++;
                }
                _resetCounter = 0;
                showHotVpns();
            }
            bool search(int vpn2, int vpn1) const
            {
                Addr cat_addr = (vpn2 << 9) | vpn1;
                Entry *entry = table.findEntry(cat_addr, true);
                if (entry) {
                    return entry->getHot();
                } else {
                    return false;
                }
                return false;
            }
            void update(int vpn2, int vpn1, bool enable_thro)
            {
                Addr cat_addr = (vpn2 << 9) | vpn1;
                Entry *entry = table.findEntry(cat_addr, true);
                if (entry && enable_thro) {
                    entry->decr();
                }
            }
            void showHotVpns()
            {
                if (GEM5_UNLIKELY(::gem5::debug::CDPHotVpns)) {
                    uint64_t valid_cnt = 0;
                    DPRINTFN("------------------------------------\n");
                    auto it = table.begin();
                    while (it != table.end()) {
                        if (it->isValid() && (it->getPrefRefCnt() > 0)) {
                            valid_cnt++;
                            DPRINTFN("Table entry(%#llx, %#llx): %#llx\n",\
                            it->debug_vpn2, it->debug_vpn1, it->getPrefRefCnt());
                        }
                        it++;
                    }
                    DPRINTFN("%ld valid entries in hotVpns\n", valid_cnt);
                    DPRINTFN("------------------------------------\n");
                }
            }
            std::string name() { return std::string("VpnTable"); }
    };

    VpnTable<VpnEntry> vpnTable;

  public:
    StatGroup *prefetchStatsPtr = nullptr;
    RequestorID parentRid;
    std::pair<long, long> l3_miss_info;  // (cdp_l3_miss,l3_total_miss_num)
    float mpki = 1;
    void setStatsPtr(StatGroup *ptr) { prefetchStatsPtr = ptr; }
    void notifyIns(int ins_num) override
    {
        if (l3_miss_info.second != 0) {
            mpki = l3_miss_info.second * 1000.0 / ins_num;
        }
    }
    bool sendPFWithFilter(Addr addr, std::vector<AddrPriority> &addresses, int prio, PrefetchSourceType pfSource,
                          int pf_depth);

    CDP(const CDPParams &p);

    ~CDP()
    {
        // Delete the pfLRUFilter pointer to release memory
        Queued::~Queued();
        delete pfLRUFilter;
    }

    ByteOrder byteOrder;

    using Queued::notifyFill;
    void notifyFill(const PacketPtr &pkt, std::vector<AddrPriority> &addresses);

    void notifyWithData(const PacketPtr &pkt, bool is_l1_use, std::vector<AddrPriority> &addresses);

    using Queued::pfHitNotify;
    void pfHitNotify(float accuracy, PrefetchSourceType pf_source, const PacketPtr &pkt,
                     std::vector<AddrPriority> &addresses);

    void calculatePrefetch(const PrefetchInfo &pfi, std::vector<AddrPriority> &addresses) override;

    void addToVpnTable(Addr vaddr);

    float getCdpTrueAccuracy() const {
        float trueAccuracy = 1;
        if (prefetchStatsPtr->pfIssued_srcs[PrefetchSourceType::CDP].value() > 100) {
            trueAccuracy = (prefetchStatsPtr->pfUseful_srcs[PrefetchSourceType::CDP].value() * 1.0) /
                            (prefetchStatsPtr->pfIssued_srcs[PrefetchSourceType::CDP].value());
        }
        return trueAccuracy;
    }

    std::vector<Addr> scanPointer(Addr addr, std::vector<uint64_t> addrs)
    {
        uint64_t test_addr;
        std::vector<Addr> ans;
        for (int of = 0; of < 8; of++) {
            test_addr = addrs[of];
            int align_bit = BITS(test_addr, 1, 0);
            int filter_bit = BITS(test_addr, 5, 0);
            int page_offset, vpn0, vpn1, vpn2, check_bit;
            check_bit = BITS(test_addr, 63, 39);
            vpn2 = BITS(test_addr, 38, 30);
            vpn1 = BITS(test_addr, 29, 21);
            vpn0 = BITS(test_addr, 20, 12);
            page_offset = BITS(test_addr, 11, 0);
            bool flag = true;
            if ((check_bit != 0) || (!vpnTable.search(vpn2, vpn1)) || (vpn0 == 0) || (align_bit != 0)) {
                flag = false;
            }
            Addr test_addr2 = Addr(test_addr);
            if (flag) {
                ans.push_back(test_addr2);
            }
        }
        return ans;
    };


    boost::compute::detail::lru_cache<Addr, Addr> *pfLRUFilter;
    std::list<DeferredPacket> localBuffer;
    unsigned depth{4};

    struct CDPStats : public statistics::Group
    {
        CDPStats(statistics::Group *parent);
        // STATS
        statistics::Scalar triggeredInRxNotify;
        statistics::Scalar triggeredInCalcPf;
        statistics::Scalar dataNotifyCalled;
        statistics::Scalar dataNotifyExitBlockNotFound;
        statistics::Scalar dataNotifyExitFilter;
        statistics::Scalar dataNotifyExitDepth;
        statistics::Scalar dataNotifyNoAddrFound;
        statistics::Scalar dataNotifyNoVA;
        statistics::Scalar dataNotifyNoData;
        statistics::Scalar missNotifyCalled;
        statistics::Scalar passedFilter;
        statistics::Scalar inserted;
    } cdpStats;
};

}  // namespace prefetch
}  // namespace gem5

#endif /* __MEM_CACHE_PREFETCH_IPCP_FIRST_LEVEL_HH__ */
