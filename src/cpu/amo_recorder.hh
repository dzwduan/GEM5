#ifndef __CPU_AMO_RECORDER_HH__
#define __CPU_AMO_RECORDER_HH__

#include <string>
#include <unordered_set>
#include <vector>

#include <base/statistics.hh>
#include <base/types.hh>
#include <cpu/pred/general_arch_db.hh>
#include <sim/arch_db.hh>

namespace gem5
{

enum AMOType
{
    FENCE = 0,
    AMOSWAP = 10,
    AMOAND = 20,
    AMOOR = 30,
    AMOXOR = 40,
    AMOOTHER = 50,
    LRSC = 100,
    LOAD = 200,
    STORE = 201
};

#define BRA_HIST 3
#define AMO_HIST 3

class BaseCPU;

class AMORecorder
{
    std::string filename;
    BaseCPU *cpu;
    DataBase amo_db;
    TraceManager *amo_trace;

    // Misc
    // std::vector<Addr> prev_pc;
    // std::vector<Addr> prev_st;
    std::vector<std::pair<Addr, Tick>> prev_bra; // Find loop right before amo
    std::vector<std::pair<Addr, Tick>> prev_amo; // Find loop right before amo
    // Record loop begin, loop iter cnt

    bool enable_ldst_recording;
    std::unordered_set<Addr> amo_locations;

    // In flight
    Cycles mem_latency;
    bool value_changed;

    struct AMOInfo
    {
        AMOType type;
        Addr pc;
        Addr paddr;
        // Cycles mem_latency;
    };

    std::vector<AMOInfo> latest_amos;

    struct AMORecord : public Record
    {
        AMORecord(AMOType type, uint64_t pc, uint64_t paddr, Cycles mem_latency,
            bool value_changed,
            std::vector<std::pair<Addr, Tick>> prev_bra) {
            _tick = curTick();
            // _uint64_data["cycles"] = cycles;
            _uint64_data["type"] = type;
            _uint64_data["pc"] = pc;
            _uint64_data["paddr"] = paddr;
            _uint64_data["mem_latency"] = mem_latency;
            _uint64_data["value_changed"] = value_changed;
            // _uint64_data["hist25"] = prev_pc.at(125);
            for (int i = 1; i <= BRA_HIST; i++) {
                // Most recent branch is stored at back
                _uint64_data["bra_addr" + std::to_string(i)] = prev_bra.at(BRA_HIST-i).first;
                _uint64_data["bra_tick" + std::to_string(i)] = prev_bra.at(BRA_HIST-i).second;
            }
        }
    };
public:

    AMORecorder(const std::string &name, BaseCPU *cpu, bool enable_ldst_recording) :
        filename(name), cpu(cpu),
        enable_ldst_recording(true) {
        warn("Initializing AMORecorder %s\n", filename.c_str());
        amo_db.init_db();

        std::vector<std::pair<std::string, DataType>> fields = {
            // {"cycles", DataType::UINT64},
            {"type", DataType::UINT64},
            {"pc", DataType::UINT64},
            {"paddr", DataType::UINT64},
            {"mem_latency", DataType::UINT64},
            {"value_changed", DataType::UINT64}
            // History 50 100
            // {"st1", DataType::UINT64},
        };
        for (int i = 1; i <= BRA_HIST; i++) {
            fields.push_back({"bra_addr" + std::to_string(i), DataType::UINT64});
            fields.push_back({"bra_tick" + std::to_string(i), DataType::UINT64});
        }
        amo_trace = amo_db.addAndGetTrace("amo_trace", fields);
        amo_trace->init_table();
    }

    ~AMORecorder() {}

    void dump() {
        amo_db.save_db(filename);
    }

    void record(AMOType type, uint64_t pc, uint64_t paddr, bool changed) {
        amo_trace->write_record(
            AMORecord(type, pc, paddr, mem_latency, changed, prev_bra)
        );

        if (type != AMOType::STORE) {
            latest_amos.push_back(AMOInfo{.type = type, .pc = pc, .paddr = paddr});
            if (latest_amos.size() > AMO_HIST) {
                latest_amos.erase(latest_amos.begin());
            }
        }
    }

    void recordStore(uint64_t pc, uint64_t paddr) {
        if (enable_ldst_recording) {
            // find latest_amos whos paddr is paddr
            for (auto it = latest_amos.end(); it != latest_amos.begin(); it--) {
                if (it->paddr == paddr) {
                    amo_trace->write_record(
                        AMORecord(AMOType::STORE, pc, paddr, mem_latency, false, prev_bra)
                    );
                    break;
                }
            }
        }
    }

    // void updatePC(Addr paddr) {
    //     prev_pc.push_back(paddr);
    //     if (prev_pc.size() > 150) {
    //         prev_pc.erase(prev_pc.begin());
    //     }
    // }

    void updateBranch(Addr paddr, Tick tick) {
        prev_bra.push_back({paddr, tick});
        if (prev_bra.size() > BRA_HIST) {
            prev_bra.erase(prev_bra.begin());
        }
    }

    // TODO This is buggy but I cannot think of an elegant way to fix
    void setLatencyStore(Cycles cycles, Addr paddr) {
        if (enable_ldst_recording) {
            for (auto it = latest_amos.end(); it != latest_amos.begin(); it--) {
                if (it->paddr == paddr) {
                    mem_latency = cycles;
                    break;
                }
            }
        }
    }

    void setLatency(Cycles cycles) {
        mem_latency = cycles;
    }

    void recordChanged(Addr paddr, bool changed) {
        value_changed = changed;
    }

    // void setLatencyWrite(Cycles cycles, Addr paddr) {
    //     if (enable_ldst_recording) {
    //         for (auto it = latest_amos.end(); it != latest_amos.begin(); it--) {
    //             if (it->paddr == paddr) {
    //                 // create a new amoinfo
    //                 break;
    //             }
    //         }
    //     }
    // }
};

}

#endif // __CPU_AMO_RECORDER_HH__
