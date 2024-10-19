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

    struct AMORecord : public Record
    {
        AMORecord(AMOType type, uint64_t pc, uint64_t vaddr) {
            _tick = curTick();
            // _uint64_data["cycles"] = cycles;
            _uint64_data["type"] = type;
            _uint64_data["pc"] = pc;
            _uint64_data["vaddr"] = vaddr;
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
            {"vaddr", DataType::UINT64}
        };
        amo_trace = amo_db.addAndGetTrace("amo_trace", fields);
        amo_trace->init_table();
    }

    ~AMORecorder() {}

    void dump() {
        amo_db.save_db(filename);
    }
    void record (AMOType type, uint64_t pc, uint64_t vaddr) {
        amo_trace->write_record(
            AMORecord(type, pc, vaddr)
        );
    }
};

}

#endif // __CPU_AMO_RECORDER_HH__
