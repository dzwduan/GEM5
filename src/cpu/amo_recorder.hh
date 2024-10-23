#ifndef __CPU_AMO_RECORDER_HH__
#define __CPU_AMO_RECORDER_HH__

#include <string>
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
    LRSC = 100
};

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
    // Record loop begin, loop iter cnt

    struct AMORecord : public Record
    {
        AMORecord(AMOType type, uint64_t pc, uint64_t vaddr, std::vector<std::pair<Addr, Tick>> prev_bra) {
            _tick = curTick();
            // _uint64_data["cycles"] = cycles;
            _uint64_data["type"] = type;
            _uint64_data["pc"] = pc;
            _uint64_data["vaddr"] = vaddr;
            // _uint64_data["hist25"] = prev_pc.at(125);
            for (int i = 1; i <= 4; i++) {
                // Most recent branch is stored at back
                _uint64_data["bra_addr" + std::to_string(i)] = prev_bra.at(4-i).first;
                _uint64_data["bra_tick" + std::to_string(i)] = prev_bra.at(4-i).second;
            }
        }
    };
public:

    AMORecorder(const std::string &name, BaseCPU *cpu) : filename(name), cpu(cpu) {
        warn("Initializing AMORecorder %s\n", filename.c_str());
        amo_db.init_db();

        std::vector<std::pair<std::string, DataType>> fields = {
            // {"cycles", DataType::UINT64},
            {"type", DataType::UINT64},
            {"pc", DataType::UINT64},
            {"vaddr", DataType::UINT64},
            // History 50 100
            // {"hist25", DataType::UINT64},
            // {"hist50", DataType::UINT64},
            // {"hist75", DataType::UINT64},
            // {"hist100", DataType::UINT64},
            // {"hist125", DataType::UINT64},
            // {"hist150", DataType::UINT64},
            // {"st1", DataType::UINT64},
        };
        for (int i = 1; i <= 4; i++) {
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
    void record (AMOType type, uint64_t pc, uint64_t vaddr) {
        amo_trace->write_record(
            AMORecord(type, pc, vaddr, prev_bra)
        );
    }

    void update_pc(Addr vaddr) {
        prev_pc.push_back(vaddr);
        if (prev_pc.size() > 150) {
            prev_pc.erase(prev_pc.begin());
        }
    }

    void update_bra(Addr vaddr, Tick tick) {
        prev_bra.push_back({vaddr, tick});
        if (prev_bra.size() > 4) {
            prev_bra.erase(prev_bra.begin());
        }
    }

    void update_store(Addr store_addr) {

    }
};

}

#endif // __CPU_AMO_RECORDER_HH__
