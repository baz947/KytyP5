#include "kyty/CompatDb.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>

namespace kyty::infra {

uint32_t CompatScore::Overall() const {
  if (dims.empty()) return 0;
  uint64_t sum = 0;
  for (auto& [_, v] : dims) sum += v;
  return uint32_t(sum / dims.size());
}

Result<void> CompatDatabase::Upsert(TitleEntry e) {
  if (e.title_id.empty())
    return Result<void>::Fail(RuntimeError::InvalidArgument, "title id");
  titles_[Key(e.title_id, e.version)] = std::move(e);
  return Result<void>::Ok();
}

Result<TitleEntry> CompatDatabase::Lookup(const std::string& title_id,
                                          const std::string& version) const {
  auto it = titles_.find(Key(title_id, version));
  if (it == titles_.end())
    return Result<TitleEntry>::Fail(RuntimeError::InvalidArgument,
                                    "unknown title");
  return Result<TitleEntry>::Ok(it->second);
}

std::vector<TitleEntry> CompatDatabase::AffectedBy(
    const std::string& subsystem) const {
  std::vector<TitleEntry> out;
  for (auto& [_, t] : titles_) {
    for (auto& i : t.issues) {
      if (i.subsystem == subsystem) {
        out.push_back(t);
        break;
      }
    }
  }
  return out;
}

Result<void> CompatDatabase::SetScore(const std::string& title_id,
                                      const std::string& version,
                                      CompatScore s) {
  if (!titles_.count(Key(title_id, version)))
    return Result<void>::Fail(RuntimeError::InvalidArgument, "unknown title");
  scores_[Key(title_id, version)] = std::move(s);
  return Result<void>::Ok();
}

Result<CompatScore> CompatDatabase::Score(const std::string& title_id,
                                          const std::string& version) const {
  auto it = scores_.find(Key(title_id, version));
  if (it == scores_.end())
    return Result<CompatScore>::Fail(RuntimeError::InvalidArgument,
                                     "no score");
  return Result<CompatScore>::Ok(it->second);
}

namespace {

// --- Minimal JSON for our fixed schema (writer + strict reader) ---
std::string JEscape(const std::string& s) {
  std::ostringstream o;
  for (char c : s) {
    switch (c) {
      case '"': o << "\\\""; break;
      case '\\': o << "\\\\"; break;
      case '\n': o << "\\n"; break;
      case '\r': o << "\\r"; break;
      case '\t': o << "\\t"; break;
      default:
        if (c >= 0 && c < 0x20) {
          char b[8];
          std::snprintf(b, sizeof(b), "\\u%04x", c);
          o << b;
        } else {
          o << c;
        }
    }
  }
  return o.str();
}

struct JVal {
  enum class T { Null, Bool, Num, Str, Arr, Obj } type = T::Null;
  bool boolean = false;
  double num = 0;
  std::string str;
  std::vector<JVal> arr;
  std::vector<std::pair<std::string, JVal>> obj;
  const JVal* Find(const std::string& k) const {
    if (type != T::Obj) return nullptr;
    for (auto& [kk, v] : obj) {
      if (kk == k) return &v;
    }
    return nullptr;
  }
};

struct JParser {
  const char* p;
  const char* end;
  std::string err;
  void Ws() {
    while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) ++p;
  }
  bool Lit(const char* s) {
    size_t n = std::strlen(s);
    if (size_t(end - p) < n || std::strncmp(p, s, n) != 0) return false;
    p += n;
    return true;
  }
  bool ParseStr(std::string& out) {
    if (p >= end || *p != '"') return false;
    ++p;
    out.clear();
    while (p < end && *p != '"') {
      if (*p == '\\') {
        ++p;
        if (p >= end) return false;
        switch (*p) {
          case '"': out += '"'; break;
          case '\\': out += '\\'; break;
          case 'n': out += '\n'; break;
          case 'r': out += '\r'; break;
          case 't': out += '\t'; break;
          case 'u': {
            if (end - p < 5) return false;
            unsigned v = 0;
            for (int i = 1; i <= 4; ++i) {
              char c = p[i];
              v <<= 4;
              if (c >= '0' && c <= '9') v += uint32_t(c - '0');
              else if (c >= 'a' && c <= 'f') v += uint32_t(c - 'a' + 10);
              else if (c >= 'A' && c <= 'F') v += uint32_t(c - 'A' + 10);
              else return false;
            }
            out += char(v < 0x80 ? v : '?');
            p += 4;
            break;
          }
          default: return false;
        }
        ++p;
      } else {
        out += *p++;
      }
    }
    if (p >= end || *p != '"') return false;
    ++p;
    return true;
  }
  bool ParseVal(JVal& v) {
    Ws();
    if (p >= end) return false;
    if (*p == '"') {
      v.type = JVal::T::Str;
      return ParseStr(v.str);
    }
    if (*p == '{') {
      v.type = JVal::T::Obj;
      ++p;
      Ws();
      if (p < end && *p == '}') {
        ++p;
        return true;
      }
      while (true) {
        Ws();
        std::string k;
        if (!ParseStr(k)) return false;
        Ws();
        if (p >= end || *p != ':') return false;
        ++p;
        JVal vv;
        if (!ParseVal(vv)) return false;
        v.obj.emplace_back(std::move(k), std::move(vv));
        Ws();
        if (p >= end) return false;
        if (*p == ',') {
          ++p;
          continue;
        }
        if (*p == '}') {
          ++p;
          return true;
        }
        return false;
      }
    }
    if (*p == '[') {
      v.type = JVal::T::Arr;
      ++p;
      Ws();
      if (p < end && *p == ']') {
        ++p;
        return true;
      }
      while (true) {
        JVal vv;
        if (!ParseVal(vv)) return false;
        v.arr.push_back(std::move(vv));
        Ws();
        if (p >= end) return false;
        if (*p == ',') {
          ++p;
          continue;
        }
        if (*p == ']') {
          ++p;
          return true;
        }
        return false;
      }
    }
    if (Lit("true")) {
      v.type = JVal::T::Bool;
      v.boolean = true;
      return true;
    }
    if (Lit("false")) {
      v.type = JVal::T::Bool;
      return true;
    }
    if (Lit("null")) {
      v.type = JVal::T::Null;
      return true;
    }
    char* e = nullptr;
    double d = std::strtod(p, &e);
    if (e == p) return false;
    p = e;
    v.type = JVal::T::Num;
    v.num = d;
    return true;
  }
};

bool JStr(const JVal* v, std::string& out) {
  if (!v || v->type != JVal::T::Str) return false;
  out = v->str;
  return true;
}
bool JNum(const JVal* v, double& out) {
  if (!v || v->type != JVal::T::Num) return false;
  out = v->num;
  return true;
}

}  // namespace

Result<void> CompatDatabase::Save(const std::string& path) const {
  std::ostringstream o;
  o << "{\"v\":1,\"titles\":[";
  bool first = true;
  for (auto& [_, t] : titles_) {
    if (!first) o << ",";
    first = false;
    o << "{\"id\":\"" << JEscape(t.title_id) << "\",\"ver\":\""
      << JEscape(t.version) << "\",\"status\":" << int(t.status)
      << ",\"req\":[" << t.requirements.wave64 << ","
      << t.requirements.dynamic_descriptors << "," << t.requirements.ray_tracing
      << "," << t.requirements.image_atomics << "," << t.requirements.bda
      << "],\"issues\":[";
    bool fi = true;
    for (auto& i : t.issues) {
      if (!fi) o << ",";
      fi = false;
      o << "{\"sub\":\"" << JEscape(i.subsystem) << "\",\"st\":" << int(i.status)
        << ",\"note\":\"" << JEscape(i.note) << "\"}";
    }
    o << "]}";
  }
  o << "],\"scores\":[";
  first = true;
  for (auto& [k, s] : scores_) {
    if (!first) o << ",";
    first = false;
    o << "{\"k\":\"" << JEscape(k) << "\",\"dims\":{";
    bool fd = true;
    for (auto& [dk, dv] : s.dims) {
      if (!fd) o << ",";
      fd = false;
      o << "\"" << JEscape(dk) << "\":" << dv;
    }
    o << "}}";
  }
  o << "]}";
  std::ofstream f(path, std::ios::binary | std::ios::trunc);
  if (!f) return Result<void>::Fail(RuntimeError::HostMemoryFailure,
                                    "cannot open " + path);
  f << o.str();
  f.flush();
  if (!f) return Result<void>::Fail(RuntimeError::HostMemoryFailure,
                                    "write failed " + path);
  return Result<void>::Ok();
}

Result<void> CompatDatabase::Load(const std::string& path) {
  std::ifstream f(path, std::ios::binary);
  if (!f)
    return Result<void>::Fail(RuntimeError::InvalidArgument,
                              "cannot open " + path);
  std::ostringstream ss;
  ss << f.rdbuf();
  std::string text = ss.str();
  JParser ps{text.data(), text.data() + text.size()};
  JVal root;
  if (!ps.ParseVal(root) || root.type != JVal::T::Obj) {
    return Result<void>::Fail(RuntimeError::InvalidArgument,
                              "corrupt compatdb (root)");
  }
  // Parse into temporaries first: DB untouched on failure.
  std::map<std::string, TitleEntry> titles;
  std::map<std::string, CompatScore> scores;
  double v = 0;
  const JVal* jv = root.Find("v");
  if (!JNum(jv, v) || v != 1) {
    return Result<void>::Fail(RuntimeError::UnsupportedFeature,
                              "compatdb version");
  }
  const JVal* jt = root.Find("titles");
  if (!jt || jt->type != JVal::T::Arr) {
    return Result<void>::Fail(RuntimeError::InvalidArgument,
                              "corrupt compatdb (titles)");
  }
  for (auto& te : jt->arr) {
    TitleEntry e;
    if (!JStr(te.Find("id"), e.title_id) || e.title_id.empty() ||
        !JStr(te.Find("ver"), e.version)) {
      return Result<void>::Fail(RuntimeError::InvalidArgument,
                                "corrupt compatdb (title)");
    }
    double st = 0;
    if (!JNum(te.Find("status"), st) || st < 0 || st > 3) {
      return Result<void>::Fail(RuntimeError::InvalidArgument,
                                "corrupt compatdb (status)");
    }
    e.status = TitleStatus(int(st));
    const JVal* rq = te.Find("req");
    if (!rq || rq->type != JVal::T::Arr || rq->arr.size() != 5) {
      return Result<void>::Fail(RuntimeError::InvalidArgument,
                                "corrupt compatdb (req)");
    }
    for (auto& r : rq->arr) {
      if (r.type != JVal::T::Num && r.type != JVal::T::Bool) {
        return Result<void>::Fail(RuntimeError::InvalidArgument,
                                  "corrupt compatdb (req val)");
      }
    }
    auto truth = [](const JVal& r) {
      return r.type == JVal::T::Bool ? r.boolean : (r.num != 0);
    };
    e.requirements.wave64 = truth(rq->arr[0]);
    e.requirements.dynamic_descriptors = truth(rq->arr[1]);
    e.requirements.ray_tracing = truth(rq->arr[2]);
    e.requirements.image_atomics = truth(rq->arr[3]);
    e.requirements.bda = truth(rq->arr[4]);
    const JVal* ji = te.Find("issues");
    if (!ji || ji->type != JVal::T::Arr) {
      return Result<void>::Fail(RuntimeError::InvalidArgument,
                                "corrupt compatdb (issues)");
    }
    for (auto& ie : ji->arr) {
      KnownIssue k;
      double kst = 0;
      if (!JStr(ie.Find("sub"), k.subsystem) ||
          !JNum(ie.Find("st"), kst) || !JStr(ie.Find("note"), k.note)) {
        return Result<void>::Fail(RuntimeError::InvalidArgument,
                                  "corrupt compatdb (issue)");
      }
      k.status = CompatStatus(int(kst));
      e.issues.push_back(std::move(k));
    }
    titles[Key(e.title_id, e.version)] = std::move(e);
  }
  const JVal* js = root.Find("scores");
  if (!js || js->type != JVal::T::Arr) {
    return Result<void>::Fail(RuntimeError::InvalidArgument,
                              "corrupt compatdb (scores)");
  }
  for (auto& se : js->arr) {
    std::string k;
    if (!JStr(se.Find("k"), k)) {
      return Result<void>::Fail(RuntimeError::InvalidArgument,
                                "corrupt compatdb (score key)");
    }
    const JVal* dm = se.Find("dims");
    if (!dm || dm->type != JVal::T::Obj) {
      return Result<void>::Fail(RuntimeError::InvalidArgument,
                                "corrupt compatdb (dims)");
    }
    CompatScore sc;
    for (auto& [dk, dv] : dm->obj) {
      double d = 0;
      if (!JNum(&dv, d) || d < 0 || d > 100) {
        return Result<void>::Fail(RuntimeError::InvalidArgument,
                                  "corrupt compatdb (dim)");
      }
      sc.dims[dk] = uint32_t(d);
    }
    scores[k] = std::move(sc);
  }
  // Trailing garbage rejected (strict).
  ps.Ws();
  if (ps.p != ps.end) {
    return Result<void>::Fail(RuntimeError::InvalidArgument,
                              "corrupt compatdb (trailing)");
  }
  titles_ = std::move(titles);
  scores_ = std::move(scores);
  return Result<void>::Ok();
}

void CompatDatabase::Clear() {
  titles_.clear();
  scores_.clear();
}
