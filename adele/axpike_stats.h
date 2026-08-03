#ifndef _AXPIKE_STATS_H_
#define _AXPIKE_STATS_H_

#include <unordered_map>
#include <string>
#include <vector>
#include <cstdint>

class processor_t;

namespace AxPIKE {

  class Stats {
    private:
      std::unordered_map<uint64_t, std::unordered_map<uint8_t, uint64_t*>> icounter_map;
      std::unordered_map<uint64_t, std::unordered_map<uint8_t, double*>> ecounter_map;

      uint64_t* icounter;
      double* ecounter;

      processor_t* p;
      uint32_t& cur_insn_id;
      uint64_t* active_approx;
      int seq;
      bool finalized;

    public:
      uint8_t section;
      Stats(processor_t* _p, uint32_t& _cur_insn_id, uint64_t* _active_approx) : p(_p), cur_insn_id(_cur_insn_id), active_approx(_active_approx), seq(0), finalized(false), section(0), max_section(0) {
        AxPIKE::Stats::initStats();
        setStats();
      };
      ~Stats();

      uint64_t instrs_counter=0;
      void finalize();
      void printCounters();
      void clearCounters();
      void printInstrCounter(const char* fname);
      void printEnergyCounter(const char* fname);
      void printTransprecisionCounter(const char* fname);
      void printTransprecisionSectionCounter(const char* fname);
      void insnDispatch(double energy);
      static void initStats();
      void setStats();
      void setSection(uint8_t new_section);
      void newSection();
      static std::vector<std::string> insns;
      static std::unordered_map<uint8_t, std::string> approxes;

      uint8_t max_section;
  };
}

#endif
