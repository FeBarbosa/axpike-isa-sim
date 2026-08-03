#include "axpike_stats.h"
#include "axpike_stats_sections.h"
#include <algorithm>
#include <iostream>
#include <fstream>
#include <unistd.h>
#include "processor.h"
#include "insn_count.h"
#include "mmu.h"
#include "transprecision_classification.h"


std::vector<std::string> AxPIKE::Stats::insns;
std::unordered_map<uint8_t, std::string> AxPIKE::Stats::approxes;

AxPIKE::Stats::~Stats() = default;

void AxPIKE::Stats::finalize() {
  if (finalized)
    return;
  printCounters();
  finalized = true;
}

void AxPIKE::Stats::printCounters() {
  std::string fname;
  fname = "AxPIKE_counters_" + std::to_string(::getpid()) + "_hart" + std::to_string(p->get_id()) + "_" + std::to_string(seq) + ".csv";
  printInstrCounter(fname.c_str());
  fname = "AxPIKE_energy_" + std::to_string(::getpid()) + "_hart" + std::to_string(p->get_id()) + "_" + std::to_string(seq) + ".csv";
  printEnergyCounter(fname.c_str());
  fname = "AxPIKE_transprecision_" + std::to_string(::getpid()) + "_hart" + std::to_string(p->get_id()) + "_" + std::to_string(seq++) + ".csv";
  printTransprecisionCounter(fname.c_str());
  fname = "AxPIKE_transprecision_sections_" + std::to_string(::getpid()) + "_hart" + std::to_string(p->get_id()) + "_" + std::to_string(seq - 1) + ".csv";
  printTransprecisionSectionCounter(fname.c_str());
  clearCounters();
}

void AxPIKE::Stats::clearCounters() {
  icounter_map.clear();
  ecounter_map.clear();
  setStats();
}

void AxPIKE::Stats::printInstrCounter(const char* fname) {
  std::ofstream fp;
  fp.open(fname, std::ios::out | std::ios::trunc);

  const char *prvs[4] = {"U", "S", "HS", "M"};

  std::cerr << "Writing AxPIKE instruction counters to " << fname << std::endl;

  if (fp.is_open()) {
    fp << "\"Instruction/Approximation\",\"No approximation\"";
    for (int i = 1; i <= max_section; i++) {
      fp << ",";
    }
    for (auto& x : icounter_map) {
      uint64_t a = x.first;
      if (a != 0x0) {
        std::string as("");
        for (auto& y: approxes) {
          if ((a & (static_cast<uint64_t>(0x01) << y.first)) != 0x0) {
            as = as + " | " + y.second;
          }
        }
        as.erase(0, 3);
        fp << ", \"" << as << "\"";
        for (int i = 1; i <= max_section; i++) {
          fp << ", ";
        }
      }
    }
    fp << std::endl;

    for (auto& y : icounter_map) {
      for (int i = 0; i <= max_section; i++) {
        fp << ",\"Section " << i << "\"";
      }
    }
    fp << std::endl;

    fp << "\"-All instructions-\"";
    uint64_t sum = 0;
    for (int i = 0; i <= max_section; i++) {
      sum = 0;
      for (uint64_t instr = 0; instr < INSN_COUNT<<2; instr++) {
        sum += detail::sectionCounterValue(icounter_map, 0x0, i, instr);
      }
      fp << ", " << sum;
    }
    for (auto& y : icounter_map) {
      if (y.first != 0x0) {
        for (int i = 0; i <= max_section; i++) {
          sum = 0;
          const uint64_t* section_counters =
              detail::findSectionCounters(icounter_map, y.first, i);
          if (section_counters != nullptr) {
            for (uint64_t instr = 0; instr < INSN_COUNT<<2; instr++) {
              sum += section_counters[instr];
            }
          }
          fp << ", " << sum;
        }
      }
    }
    fp << std::endl;

    for (int prv = 0; prv < 4; prv++) {
      fp << "\"-All|" << prvs[prv] << "-\"";
      for (int i = 0; i <= max_section; i++) {
        sum = 0;
        for (uint64_t instr = prv; instr < INSN_COUNT<<2; instr+=4) {
          sum += detail::sectionCounterValue(icounter_map, 0x0, i, instr);
        }
        fp << ", " << sum;
      }
      for (auto& y : icounter_map) {
        if (y.first != 0x0) {
          for (int i = 0; i <= max_section; i++) {
            sum = 0;
            const uint64_t* section_counters =
                detail::findSectionCounters(icounter_map, y.first, i);
            if (section_counters != nullptr) {
              for (uint64_t instr = prv; instr < INSN_COUNT<<2; instr+=4) {
                sum += section_counters[instr];
              }
            }
            fp << ", " << sum;
          }
        }
      }
      fp << std::endl;
    }

    for (uint64_t instr = 0; instr < INSN_COUNT<<2; instr++) {
      fp <<"\"" << AxPIKE::Stats::insns[instr>>2] << "|" << prvs[instr&0x3] << "\"";
      for (int i = 0; i <= max_section; i++) {
        fp << ", "
           << detail::sectionCounterValue(icounter_map, 0x0, i, instr);
      }
      for (auto& y : icounter_map) {
        if (y.first != 0x0) {
          for (int i = 0; i <= max_section; i++) {
            const uint64_t* section_counters =
                detail::findSectionCounters(icounter_map, y.first, i);
            if (section_counters != nullptr) {
              fp << ", " << section_counters[instr];
            }
            else {
              fp << ", 0";
            }
          }
        }
      }
      fp << std::endl;
    }

    fp.close();
  }
  else {
    std::cerr << "AxPIKE Error: Unable to open file \"" << fname << "\" to save instruction counters." << std::endl;
  }
}

void AxPIKE::Stats::printEnergyCounter(const char* fname) {
  std::ofstream fp;
  
  const char *prvs[4] = {"U", "S", "HS", "M"};

  fp.open(fname, std::ios::out | std::ios::trunc);
  std::cerr << "Writing AxPIKE energy counters to " << fname << std::endl;

  if (fp.is_open()) {
    fp << "\"Instruction/Approximation\",\"No approximation\"";
    for (int i = 1; i <= max_section; i++) {
      fp << ",";
    }
    for (auto& x : ecounter_map) {
      uint64_t a = x.first;
      if (a != 0x0) {
        std::string as("");
        for (auto& y: approxes) {
          if ((a & (static_cast<uint64_t>(0x01) << y.first)) != 0x0) {
            as = as + " | " + y.second;
          }
        }
        as.erase(0, 3);
        fp << ", \"" << as << "\"";
        for (int i = 1; i <= max_section; i++) {
          fp << ", ";
        }
      }
    }
    fp << std::endl;

    for (auto& y : ecounter_map) {
      for (int i = 0; i <= max_section; i++) {
        fp << ",\"Section " << i << "\"";
      }
    }
    fp << std::endl;

    fp << "\"-All instructions-\"";
    double sum = 0;
    for (int i = 0; i <= max_section; i++) {
      sum = 0;
      for (uint64_t instr = 0; instr < INSN_COUNT<<2; instr++) {
        sum += detail::sectionCounterValue(ecounter_map, 0x0, i, instr);
      }
      fp << ", " << sum;
    }
    for (auto& y : ecounter_map) {
      if (y.first != 0x0) {
        for (int i = 0; i <= max_section; i++) {
          sum = 0;
          const double* section_counters =
              detail::findSectionCounters(ecounter_map, y.first, i);
          if (section_counters != nullptr) {
            for (uint64_t instr = 0; instr < INSN_COUNT<<2; instr++) {
              sum += section_counters[instr];
            }
          }
          fp << ", " << sum;
        }
      }
    }
    fp << std::endl;

    for (int prv = 0; prv < 4; prv++) {
      fp << "\"-All|" << prvs[prv] << "-\"";
      for (int i = 0; i <= max_section; i++) {
        sum = 0;
        for (uint64_t instr = prv; instr < INSN_COUNT<<2; instr+=4) {
          sum += detail::sectionCounterValue(ecounter_map, 0x0, i, instr);
        }
        fp << ", " << sum;
      }
      for (auto& y : ecounter_map) {
        if (y.first != 0x0) {
          for (int i = 0; i <= max_section; i++) {
            sum = 0;
            const double* section_counters =
                detail::findSectionCounters(ecounter_map, y.first, i);
            if (section_counters != nullptr) {
              for (uint64_t instr = prv; instr < INSN_COUNT<<2; instr+=4) {
                sum += section_counters[instr];
              }
            }
            fp << ", " << sum;
          }
        }
      }
      fp << std::endl;
    }

    for (uint64_t instr = 0; instr < INSN_COUNT<<2; instr++) {
      fp <<"\"" << AxPIKE::Stats::insns[instr>>2] << "|" << prvs[instr&0x3] << "\"";
      for (int i = 0; i <= max_section; i++) {
        fp << ", "
           << detail::sectionCounterValue(ecounter_map, 0x0, i, instr);
      }
      for (auto& y : ecounter_map) {
        if (y.first != 0x0) {
          for (int i = 0; i <= max_section; i++) {
            const double* section_counters =
                detail::findSectionCounters(ecounter_map, y.first, i);
            if (section_counters != nullptr) {
              fp << ", " << section_counters[instr];
            }
            else {
              fp << ", 0.0";
            }
          }
        }
      }
      fp << std::endl;
    }

    fp.close();
  }
  else {
    std::cerr << "AxPIKE Error: Unable to open file \"" << fname << "\" to save energy counters." << std::endl;
  }
}

void AxPIKE::Stats::printTransprecisionCounter(const char* fname) {
  std::ofstream fp;

  fp.open(fname, std::ios::out | std::ios::trunc);
  std::cerr << "Writing AxPIKE transprecision counters to " << fname
            << std::endl;

  if (fp.is_open()) {
    const auto& counters = p->state.transprecision_counters;
    const auto& policy = p->get_cfg().transprecision_policy;

    fp << "\"Category\",\"Instruction\",\"From\",\"To\",\"Type\",\"Class\",\"Value\""
       << std::endl;
    fp << "\"policy_version\",\"\",\"\",\"\","
       << "\"effective-type-quantization-v4\",\"\",4"
       << std::endl;
    fp << "\"policy_protected_bits\",\"\",\"\",\"\",\"FP64\",\"\","
       << static_cast<unsigned>(policy.fp64_protected_bits)
       << std::endl;
    fp << "\"policy_protected_bits\",\"\",\"\",\"\",\"FP32\",\"\","
       << static_cast<unsigned>(policy.fp32_protected_bits)
       << std::endl;
    fp << "\"policy_protected_bits\",\"\",\"\",\"\",\"FP16\",\"\","
       << static_cast<unsigned>(policy.fp16_protected_bits)
       << std::endl;
    fp << "\"policy_protected_bits\",\"\",\"\",\"\",\"E5M2\",\"\","
       << static_cast<unsigned>(policy.e5m2_protected_bits)
       << std::endl;
    fp << "\"operand_unclassified_total\", "
       << "\"\",\"\",\"\",\"\",\"\", "
       << counters.operand_unclassified_total << std::endl;
    fp << "\"result_tag_reduction_to_zero_total\", "
       << "\"\",\"\",\"\",\"\",\"\", "
       << counters.result_tag_reduction_to_zero_total << std::endl;
    fp << "\"external_write_masked_to_zero_total\", "
       << "\"\",\"\",\"\",\"\",\"\", "
       << counters.external_write_masked_to_zero_total << std::endl;
    fp << "\"invalid_result_promotion_total\", "
       << "\"\",\"\",\"\",\"\",\"\", "
       << counters.invalid_result_promotion_total << std::endl;
    fp << "\"lazy_reclassification_total\", "
       << "\"\",\"\",\"\",\"\",\"\", "
       << counters.lazy_reclassification_total << std::endl;
    fp << "\"unclassified_fallback_total\", "
       << "\"\",\"\",\"\",\"\",\"\", "
       << counters.unclassified_fallback_total << std::endl;
    fp << "\"fp64_load_nan_boxed_fp32_effective_total\", "
       << "\"\",\"\",\"\",\"\",\"\", "
       << counters.fp64_load_nan_boxed_fp32_effective_total << std::endl;

    for (size_t type = 0; type < transprecision_type_bucket_count; type++) {
      fp << "\"write_tag_total\", "
         << "\"\",\"\",\"\",\"" << transprecision_type_bucket_name(type)
         << "\",\"\", " << counters.write_tag_total[type]
         << std::endl;
    }

    for (size_t value_class = 0;
         value_class < transprecision_value_class_bucket_count;
         value_class++) {
      fp << "\"operation_result_class_total\", "
         << "\"\",\"\",\"\",\"\",\""
         << transprecision_value_class_name(value_class)
         << "\", "
         << counters.operation_result_class_total[value_class]
         << std::endl;
      fp << "\"external_write_class_total\", "
         << "\"\",\"\",\"\",\"\",\""
         << transprecision_value_class_name(value_class)
         << "\", "
         << counters.external_write_class_total[value_class]
         << std::endl;
    }

    const size_t supported_type_count =
        transprecision_type_bucket_count - 1;
    const size_t fp32_bucket =
        transprecision_type_bucket(transprecision_type_t::FP32);
    const size_t fp64_bucket =
        transprecision_type_bucket(transprecision_type_t::FP64);
    for (size_t carrier : {fp32_bucket, fp64_bucket}) {
      for (size_t effective = 0; effective < carrier; effective++) {
        fp << "\"result_quantization_total_from_to\", "
           << "\"\",\"" << transprecision_type_bucket_name(carrier)
           << "\",\"" << transprecision_type_bucket_name(effective)
           << "\",\"\",\"\", "
           << counters.result_quantization_total_from_to[
                  carrier][effective] << std::endl;
        fp << "\"result_quantization_changed_from_to\", "
           << "\"\",\"" << transprecision_type_bucket_name(carrier)
           << "\",\"" << transprecision_type_bucket_name(effective)
           << "\",\"\",\"\", "
           << counters.result_quantization_changed_from_to[
                  carrier][effective] << std::endl;
        fp << "\"result_quantization_to_zero_from_to\", "
           << "\"\",\"" << transprecision_type_bucket_name(carrier)
           << "\",\"" << transprecision_type_bucket_name(effective)
           << "\",\"\",\"\", "
           << counters.result_quantization_to_zero_from_to[
                  carrier][effective] << std::endl;
        fp << "\"result_quantization_overflow_from_to\", "
           << "\"\",\"" << transprecision_type_bucket_name(carrier)
           << "\",\"" << transprecision_type_bucket_name(effective)
           << "\",\"\",\"\", "
           << counters.result_quantization_overflow_from_to[
                  carrier][effective] << std::endl;
        fp << "\"result_quantization_underflow_from_to\", "
           << "\"\",\"" << transprecision_type_bucket_name(carrier)
           << "\",\"" << transprecision_type_bucket_name(effective)
           << "\",\"\",\"\", "
           << counters.result_quantization_underflow_from_to[
                  carrier][effective] << std::endl;
      }
    }

    for (size_t from = 0; from < supported_type_count; from++) {
      for (size_t to = 0; to < supported_type_count; to++) {
        if (from < to) {
          fp << "\"operand_promotion_from_to\", "
             << "\"\",\"" << transprecision_type_bucket_name(from)
             << "\",\"" << transprecision_type_bucket_name(to)
             << "\",\"\",\"\", "
             << counters.operand_promotion_from_to[from][to] << std::endl;
        }
        else if (from > to) {
          fp << "\"result_tag_reduction_total_from_to\", "
             << "\"\",\"" << transprecision_type_bucket_name(from)
             << "\",\"" << transprecision_type_bucket_name(to)
             << "\",\"\",\"\", "
             << counters.result_tag_reduction_total_from_to[from][to]
             << std::endl;
          fp << "\"result_tag_reduction_changed_from_to\", "
             << "\"\",\"" << transprecision_type_bucket_name(from)
             << "\",\"" << transprecision_type_bucket_name(to)
             << "\",\"\",\"\", "
             << counters.result_tag_reduction_changed_from_to[from][to]
             << std::endl;
          if (from == fp32_bucket || from == fp64_bucket) {
            fp << "\"external_write_masked_from_to\", "
               << "\"\",\"" << transprecision_type_bucket_name(from)
               << "\",\"" << transprecision_type_bucket_name(to)
               << "\",\"\",\"\", "
               << counters.external_write_masked_from_to[from][to]
               << std::endl;
          }
        }
      }
    }

    const size_t instruction_count =
        std::min(counters.effective_type_by_instruction.size(),
                 AxPIKE::Stats::insns.size());
    for (size_t instr = 0; instr < instruction_count; instr++) {
      for (size_t type = 0; type < transprecision_type_bucket_count; type++) {
        const uint64_t count =
            counters.effective_type_by_instruction[instr][type];
        if (count == 0)
          continue;
        fp << "\"effective_type_by_instruction\", "
           << "\"" << AxPIKE::Stats::insns[instr] << "\",\"\",\"\",\""
           << transprecision_type_bucket_name(type)
           << "\",\"\", " << count << std::endl;
      }
    }

    fp.close();
  }
  else {
    std::cerr << "AxPIKE Error: Unable to open file \"" << fname
              << "\" to save transprecision counters." << std::endl;
  }
}

static void writeTransprecisionSectionRows(std::ostream& fp,
    size_t section, const transprecision_counter_values_t& counters)
{
  const auto row = [&fp, section](const char* category,
      const std::string& instruction, const char* from, const char* to,
      const char* type, const char* value_class, uint64_t value) {
    fp << section << ",\"" << category << "\",\"" << instruction
       << "\",\"" << from << "\",\"" << to << "\",\"" << type
       << "\",\"" << value_class << "\"," << value << std::endl;
  };

  row("operand_unclassified_total", "", "", "", "", "",
      counters.operand_unclassified_total);
  row("result_tag_reduction_to_zero_total", "", "", "", "", "",
      counters.result_tag_reduction_to_zero_total);
  row("external_write_masked_to_zero_total", "", "", "", "", "",
      counters.external_write_masked_to_zero_total);
  row("invalid_result_promotion_total", "", "", "", "", "",
      counters.invalid_result_promotion_total);
  row("lazy_reclassification_total", "", "", "", "", "",
      counters.lazy_reclassification_total);
  row("unclassified_fallback_total", "", "", "", "", "",
      counters.unclassified_fallback_total);
  row("fp64_load_nan_boxed_fp32_effective_total", "", "", "", "", "",
      counters.fp64_load_nan_boxed_fp32_effective_total);

  for (size_t type = 0; type < transprecision_type_bucket_count; type++) {
    row("write_tag_total", "", "", "",
        transprecision_type_bucket_name(type), "",
        counters.write_tag_total[type]);
  }
  for (size_t value_class = 0;
       value_class < transprecision_value_class_bucket_count; value_class++) {
    row("operation_result_class_total", "", "", "", "",
        transprecision_value_class_name(value_class),
        counters.operation_result_class_total[value_class]);
    row("external_write_class_total", "", "", "", "",
        transprecision_value_class_name(value_class),
        counters.external_write_class_total[value_class]);
  }

  const size_t supported_type_count = transprecision_type_bucket_count - 1;
  const size_t fp32_bucket =
      transprecision_type_bucket(transprecision_type_t::FP32);
  const size_t fp64_bucket =
      transprecision_type_bucket(transprecision_type_t::FP64);
  for (size_t carrier : {fp32_bucket, fp64_bucket}) {
    for (size_t effective = 0; effective < carrier; effective++) {
      const char* from = transprecision_type_bucket_name(carrier);
      const char* to = transprecision_type_bucket_name(effective);
      row("result_quantization_total_from_to", "", from, to, "", "",
          counters.result_quantization_total_from_to[carrier][effective]);
      row("result_quantization_changed_from_to", "", from, to, "", "",
          counters.result_quantization_changed_from_to[carrier][effective]);
      row("result_quantization_to_zero_from_to", "", from, to, "", "",
          counters.result_quantization_to_zero_from_to[carrier][effective]);
      row("result_quantization_overflow_from_to", "", from, to, "", "",
          counters.result_quantization_overflow_from_to[carrier][effective]);
      row("result_quantization_underflow_from_to", "", from, to, "", "",
          counters.result_quantization_underflow_from_to[carrier][effective]);
    }
  }

  for (size_t from = 0; from < supported_type_count; from++) {
    for (size_t to = 0; to < supported_type_count; to++) {
      const char* from_name = transprecision_type_bucket_name(from);
      const char* to_name = transprecision_type_bucket_name(to);
      if (from < to) {
        row("operand_promotion_from_to", "", from_name, to_name, "", "",
            counters.operand_promotion_from_to[from][to]);
      }
      else if (from > to) {
        row("result_tag_reduction_total_from_to", "", from_name, to_name,
            "", "", counters.result_tag_reduction_total_from_to[from][to]);
        row("result_tag_reduction_changed_from_to", "", from_name, to_name,
            "", "", counters.result_tag_reduction_changed_from_to[from][to]);
        if (from == fp32_bucket || from == fp64_bucket) {
          row("external_write_masked_from_to", "", from_name, to_name,
              "", "", counters.external_write_masked_from_to[from][to]);
        }
      }
    }
  }

  const size_t instruction_count =
      std::min(counters.effective_type_by_instruction.size(),
               AxPIKE::Stats::insns.size());
  for (size_t instr = 0; instr < instruction_count; instr++) {
    for (size_t type = 0; type < transprecision_type_bucket_count; type++) {
      const uint64_t count = counters.effective_type_by_instruction[instr][type];
      if (count != 0) {
        row("effective_type_by_instruction", AxPIKE::Stats::insns[instr],
            "", "", transprecision_type_bucket_name(type), "", count);
      }
    }
  }
}

void AxPIKE::Stats::printTransprecisionSectionCounter(const char* fname) {
  std::ofstream fp(fname, std::ios::out | std::ios::trunc);
  std::cerr << "Writing AxPIKE transprecision section counters to " << fname
            << std::endl;
  if (!fp.is_open()) {
    std::cerr << "AxPIKE Error: Unable to open file \"" << fname
              << "\" to save transprecision section counters." << std::endl;
    return;
  }

  fp << "\"Section\",\"Category\",\"Instruction\",\"From\",\"To\","
        "\"Type\",\"Class\",\"Value\"" << std::endl;
  const auto& sections = p->state.transprecision_counters.section_counters;
  for (const auto& section_entry : sections) {
    writeTransprecisionSectionRows(
        fp, section_entry.first, section_entry.second);
  }
}

void AxPIKE::Stats::insnDispatch(double energy) {
  instrs_counter++;

  uint64_t instr = (cur_insn_id<<2) | p->state.prv;
  (icounter[instr])++;
  (ecounter[instr]) += energy;
}

void AxPIKE::Stats::setStats() {
  uint64_t& prv = p->state.prv;
  icounter = icounter_map[active_approx[prv]][section];
  ecounter = ecounter_map[active_approx[prv]][section];
  if (!icounter) {
    icounter = new uint64_t[INSN_COUNT<<2];
    ecounter = new double[INSN_COUNT<<2];
    for (int i = 0; i < (INSN_COUNT<<2); i++) {
      icounter[i] = 0;
      ecounter[i] = 0.0;
    }
    icounter_map[active_approx[prv]][section] = icounter;
    ecounter_map[active_approx[prv]][section] = ecounter;
  }
}

void AxPIKE::Stats::setSection(uint8_t new_section) {
  section = new_section;
  max_section = std::max(max_section, section);
  setStats();
  p->state.transprecision_counters.set_section(section);
}

void AxPIKE::Stats::newSection() {
  setSection(static_cast<uint8_t>(section + 1));
}
