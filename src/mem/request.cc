#include "mem/request.hh"

#include "cpu/base.hh"

namespace gem5
{
int Request::getCPUId() const { return _cpu->cpuId(); }

BaseCPU* Request::getCPU() const { return _cpu; }

void Request::setCPU(BaseCPU *cpu) { _cpu = cpu; }
} // namespace gem5
