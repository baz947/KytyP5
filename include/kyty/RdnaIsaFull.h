#pragma once
// Full RDNA ISA tables — extends IsaTable with the complete MIMG/MTBUF/
// MUBUF/SMEM/SOPP/SALU/VOP/VOPC/DS/FLAT classes relevant to PS5 shaders.
// Each variant maps to a pipeline Op class so the decoder stays exact
// while coverage is measured per real variant (not per class).
#include <string>
#include <vector>
#include "kyty/Compat.h"
#include "kyty/IsaTable.h"
#include "kyty/ShaderCore.h"

namespace kyty::shader {

class RdnaIsaFull {
 public:
  RdnaIsaFull();
  [[nodiscard]] const std::vector<IsaEntry>& entries() const {
    return entries_;
  }
  void InstallAll(CapabilityRegistry& reg) const;
  // Per-class coverage at a stage: {"mimg": 1.0, "ds": 0.9, ...}.
  [[nodiscard]] std::vector<std::pair<std::string, double>> CoverageByClass(
      Stage stage) const;
  [[nodiscard]] double CoverageAt(Stage stage) const;
  const IsaEntry* FindByName(const std::string& name) const;

 private:
  std::vector<IsaEntry> entries_;
};

}  // namespace kyty::shader
