#include <cassert>
#include <cstdio>
#include "kyty/CompatDb.h"
#include "kyty/CorpusRunner.h"
#include "kyty/HostGpuCaps.h"
#include "kyty/SoakRunner.h"

int main() {
  using namespace kyty;
  using namespace kyty::infra;

  // CompatDB durability: roundtrip + corrupt rejected, DB untouched.
  CompatDatabase db;
  TitleEntry e;
  e.title_id = "PPSA00001";
  e.version = "01.00";
  e.status = TitleStatus::Ingame;
  e.requirements.bda = true;
  e.issues.push_back(KnownIssue{"guest_memory", CompatStatus::Broken, "n\"ote"});
  assert(db.Upsert(e).ok());
  CompatScore s;
  s.dims = {{"Boot", 100}, {"Gameplay", 40}};
  assert(db.SetScore("PPSA00001", "01.00", s).ok());
  const char* path = "soak_test_compatdb.json";
  assert(db.Save(path).ok());
  CompatDatabase db2;
  assert(db2.Load(path).ok());
  auto t = db2.Lookup("PPSA00001", "01.00");
  assert(t.ok() && t.value.issues.size() == 1);
  assert(t.value.issues[0].note == "n\"ote");  // escaping roundtrips
  assert(db2.Score("PPSA00001", "01.00").ok().value.Overall() == 70);
  assert(db2.AffectedBy("guest_memory").size() == 1);
  // Corrupt file: structured error, previous contents kept.
  {
    FILE* f = std::fopen("soak_test_bad.json", "w");
    std::fputs("{\"v\":1,\"titles\":[{\"id\":", f);
    std::fclose(f);
  }
  assert(!db2.Load("soak_test_bad.json").ok());
  assert(db2.Lookup("PPSA00001", "01.00").ok());  // untouched
  assert(!db2.Load("no/such/file.json").ok());
  std::remove(path);
  std::remove("soak_test_bad.json");

  // Soak: calm plan passes oracle; storm files failures into the DB.
  gpu::HostGpuCaps caps(gpu::HostFeatures{});
  CompatDatabase sdb;
  TitleEntry se;
  se.title_id = "PPSA00002";
  se.version = "01.00";
  assert(sdb.Upsert(se).ok());
  auto oracle = [](const CorpusCase& c) -> Result<std::string> {
    return Result<std::string>::Ok(c.expected);
  };
  SoakRunner calm(&sdb, ChaosInjector(ChaosPlan{}));
  auto r1 = calm.Run("PPSA00002", "01.00", BuiltinCorpus(), oracle, caps);
  assert(r1.ok() && r1.value.failed == 0 && r1.value.filed == 0);

  SoakRunner storm(&sdb, ChaosInjector(ChaosPlan{{ChaosFault::DeviceLost}}));
  auto r2 = storm.Run("PPSA00002", "01.00", BuiltinCorpus(), oracle, caps);
  assert(r2.ok() && r2.value.failed > 0);
  assert(r2.value.filed == r2.value.failed);
  assert(!sdb.AffectedBy("videoout").empty());  // filed evidence present

  // Unknown title -> structured error, no crash.
  SoakRunner s3(&sdb, ChaosInjector(ChaosPlan{}));
  assert(!s3.Run("GHOST", "00.00", BuiltinCorpus(), oracle, caps).ok());

  std::puts("test_soak OK");
  return 0;
}
