#ifndef __CPU_PRED_TIMED_PRED_HH__
#define __CPU_PRED_TIMED_PRED_HH__


#include <boost/dynamic_bitset.hpp>

#include "base/types.hh"
#include "cpu/inst_seq.hh"
#include "cpu/pred/stream_struct.hh"
#include "params/TimedPredictor.hh"
#include "sim/sim_object.hh"

namespace gem5
{

namespace branch_prediction
{

class TimedPredictor: public SimObject
{
    public:

    typedef TimedPredictorParams Params;

    TimedPredictor(const Params &params);

    virtual void tickStart() {}
    virtual void tick() {}
    virtual void putPCHistory(Addr pc, Addr curChunkStart,
                              const boost::dynamic_bitset<> &history) {}

    virtual StreamPrediction getStream() { panic("Not implemented"); }

    virtual unsigned getDelay() {return 0;}

};

} // namespace branch_prediction

} // namespace gem5

#endif // __CPU_PRED_TIMED_PRED_HH__