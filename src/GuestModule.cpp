#include "kyty/GuestModule.h"

namespace kyty::linker {

bool GuestModule::CanTransitionTo(ModuleState next) const {
  if (state == ModuleState::Failed || state == ModuleState::Unloaded)
    return false;
  // Linear pipeline with terminal branches.
  switch (state) {
    case ModuleState::Discovered:
      return next == ModuleState::Loading || next == ModuleState::Failed;
    case ModuleState::Loading:
      return next == ModuleState::Mapped || next == ModuleState::Failed;
    case ModuleState::Mapped:
      return next == ModuleState::Parsed || next == ModuleState::Failed;
    case ModuleState::Parsed:
      return next == ModuleState::DependenciesResolved ||
             next == ModuleState::Failed;
    case ModuleState::DependenciesResolved:
      return next == ModuleState::Relocated || next == ModuleState::Failed;
    case ModuleState::Relocated:
      return next == ModuleState::Initialized || next == ModuleState::Failed;
    case ModuleState::Initialized:
      return next == ModuleState::Running || next == ModuleState::Failed;
    case ModuleState::Running:
      return next == ModuleState::Stopping || next == ModuleState::Failed;
    case ModuleState::Stopping:
      return next == ModuleState::Unloaded || next == ModuleState::Failed;
    default:
      return false;
  }
}

}  // namespace kyty::linker
