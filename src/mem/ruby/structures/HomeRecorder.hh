#ifndef __MEM_RUBY_STRUCTURES_HOMERECORDER_HH__
#define __MEM_RUBY_STRUCTURES_HOMERECORDER_HH__

#include <string>
#include <unordered_map>
#include <vector>

#include <base/statistics.hh>
#include <base/types.hh>
#include <cpu/pred/general_arch_db.hh>
#include <sim/arch_db.hh>

namespace gem5
{

namespace ruby
{

class HomeRecorder
{
    std::string filename;
    DataBase home_db;
    TraceManager *home_trace;

    // Misc
    // std::vector<Addr> prev_pc;
    // std::vector<Addr> prev_st;
    // Record loop begin, loop iter cnt

    bool enable_ldst_recording;

    struct HomeRecord : public Record
    {
        // Failed CleanUnique + ReadUnique causes this.
        bool double_req;

        HomeRecord(Addr paddr, int cpuid, Tick arrival_tick) {
            _tick = curTick();
            _uint64_data["paddr"] = paddr;
            _uint64_data["cpuid"] = cpuid;
            _uint64_data["arrival_tick"] = arrival_tick;
            _uint64_data["begin_tick"] = arrival_tick;
            _uint64_data["completion_tick"] = 0;
            double_req = false;
        }

        void setBeginTick(Tick begin_tick) {
            _uint64_data["begin_tick"] = begin_tick;
        }

        void setCompletionTick(Tick completion_tick) {
            _uint64_data["completion_tick"] = completion_tick;
        }
    };

    typedef std::unordered_map<Addr, HomeRecord> PerCPURecord;
    std::unordered_map<int, PerCPURecord> pending_records;

public:

    HomeRecorder(const std::string &name) :
        filename(name) {
        warn("Initializing HomeRecorder %s\n", filename.c_str());
        home_db.init_db();

        std::vector<std::pair<std::string, DataType>> fields = {
            {"paddr", DataType::UINT64},
            {"cpuid", DataType::UINT64},
            {"arrival_tick", DataType::UINT64},
            {"begin_tick", DataType::UINT64},
            {"completion_tick", DataType::UINT64},
        };

        home_trace = home_db.addAndGetTrace("home_trace", fields);
        home_trace->init_table();
    }

    ~HomeRecorder() {}

    void dump() {
        home_db.save_db(filename);
    }

    void record_arrival(Addr addr, int cpuid) {
        PerCPURecord& this_cpu_record = pending_records[cpuid];
        if (GEM5_UNLIKELY(this_cpu_record.find(addr) != this_cpu_record.end())) {
            warn("HomeRecorder: At arrival: at addr %#lx cpuid %d,"
                "there's already one request!\n", addr, cpuid);
            if (this_cpu_record.at(addr).double_req) {
                fatal("HomeRecorder: At arrival: at addr %#lx cpuid %d, TRIPLE REQ?! WTF?\n", addr, cpuid);
            }
            this_cpu_record.at(addr).double_req = true;
        } else {
            // warn("HomeRecorder: At arrival: at addr %#lx cpuid %d\n", addr, cpuid);
            this_cpu_record.emplace(addr, HomeRecord(addr, cpuid, curTick()));
        }
    }

    void record_begin_process(Addr addr, int cpuid) {
        auto this_cpu_record = pending_records.find(cpuid);
        if (GEM5_UNLIKELY(this_cpu_record == pending_records.end())) {
            fatal("HomeRecorder: At begin process: No record for cpuid %d\n", cpuid);
        }
        auto this_record = (this_cpu_record->second).find(addr);
        if (GEM5_UNLIKELY(this_record == this_cpu_record->second.end())) {
            fatal("HomeRecorder: At begin process: No record for addr %#lx cpuid %d\n", addr, cpuid);
        }
        this_record->second.setBeginTick(curTick());
    }

    void record_completion(Addr addr, int cpuid) {
        auto this_cpu_record = pending_records.find(cpuid);
        if (GEM5_UNLIKELY(this_cpu_record == pending_records.end())) {
            fatal("HomeRecorder: At completion: No record for cpuid %d\n", cpuid);
        }
        auto this_record = (this_cpu_record->second).find(addr);
        if (GEM5_UNLIKELY(this_record == this_cpu_record->second.end())) {
            fatal("HomeRecorder: At completion: No record for addr %#lx cpuid %d\n", addr, cpuid);
        }

        if (GEM5_UNLIKELY(this_record->second.double_req)) {
            this_record->second.double_req = false;
        } else {
            this_record->second.setCompletionTick(curTick());
            home_trace->write_record(this_record->second);
            this_cpu_record->second.erase(this_record);
        }
        // warn("HomeRecorder: deleting entry for addr #%lx cpuid %d\n", addr, cpuid);
    }
};

}

}

#endif // __MEM_RUBY_STRUCTURES_HOMERECORDER_HH__
