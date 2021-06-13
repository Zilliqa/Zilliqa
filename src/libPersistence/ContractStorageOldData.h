/*
 * Copyright (C) 2020 Zilliqa
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef ZILLIQA_SRC_LIBPERSISTENCE_CONTRACTSTORAGEOLDDATA_H_
#define ZILLIQA_SRC_LIBPERSISTENCE_CONTRACTSTORAGEOLDDATA_H_

#include <leveldb/db.h>

#include "depends/libDatabase/LevelDB.h"
#include "libUtils/DataConversion.h"

namespace Contract {

enum FINDRESULT { FOUND, NOTFOUND, DELETED };

struct MapBase {
  virtual FINDRESULT exists(dev::h256 const& _h) const = 0;
  virtual std::string lookup(dev::h256 const& _h) const = 0;
  virtual void insert(dev::h256 const& _h, dev::bytesConstRef _v) = 0;
  virtual bool kill(dev::h256 const& _h) = 0;
};

template <typename ADDS, typename DELETES>
struct AddDeleteMap : private MapBase {
  AddDeleteMap(std::shared_ptr<ADDS> adds, std::shared_ptr<DELETES> deletes)
      : m_adds(adds), m_deletes(deletes) {}
  FINDRESULT exists(dev::h256 const& _h) const {
    if (m_deletes->find(_h) != m_deletes->end()) {
      return DELETED;
    }
    return m_adds->find(_h) != m_adds->end() ? FOUND : NOTFOUND;
  }
  std::string lookup(dev::h256 const& _h) const {
    if (m_deletes->find(_h) != m_deletes->end()) {
      return "";
    }
    auto find = m_adds->find(_h);
    if (find != m_adds->end()) {
      return DataConversion::CharArrayToString(find->second);
    }
    return "";
  }
  virtual void insert(dev::h256 const& _h, dev::bytesConstRef _v) {
    auto find = m_deletes->find(_h);
    if (find != m_deletes->end()) {
      m_deletes->erase(find);
    }

    (*m_adds)[_h] = _v.toBytes();
  }
  virtual bool kill(dev::h256 const& _h) {
    auto find = m_adds->find(_h);
    if (find != m_adds->end()) {
      m_adds->erase(find);
      return true;
    }

    m_deletes->emplace(_h);
    return true;
  }
  virtual void reset() {
    m_adds->clear();
    m_deletes->clear();
  }

 protected:
  std::shared_ptr<ADDS> m_adds;
  std::shared_ptr<DELETES> m_deletes;
};

template <typename ADDS, typename DELETES>
struct RecordableAddDeleteMap : public AddDeleteMap<ADDS, DELETES> {
  RecordableAddDeleteMap(std::shared_ptr<ADDS> adds,
                         std::shared_ptr<DELETES> deletes)
      : AddDeleteMap<ADDS, DELETES>(adds, deletes) {}
  void insert(dev::h256 const& _h, dev::bytesConstRef _v) override {
    if (m_recording) {
      FINDRESULT found = AddDeleteMap<ADDS, DELETES>::exists(_h);
      if (found == DELETED) {
        r_deletes.emplace(_h);
      } else {
        bytes r_val;
        if (found == FOUND) {
          r_val = DataConversion::StringToCharArray(
              AddDeleteMap<ADDS, DELETES>::lookup(_h));
        }  // else (NOTFOUND) r_val is empty
        r_adds.emplace(_h, r_val);
      }
    }

    AddDeleteMap<ADDS, DELETES>::insert(_h, _v);
  }
  bool kill(dev::h256 const& _h) override {
    if (m_recording) {
      FINDRESULT findresult = AddDeleteMap<ADDS, DELETES>::exists(_h);
      if (findresult == FOUND) {
        r_adds.emplace(_h, DataConversion::StringToCharArray(
                               AddDeleteMap<ADDS, DELETES>::lookup(_h)));
      } else if (findresult == NOTFOUND) {
        r_newdeletes.emplace(_h);
      }
    }

    return AddDeleteMap<ADDS, DELETES>::kill(_h);
  }

  void reset() override {
    r_adds.clear();
    r_deletes.clear();
    r_newdeletes.clear();
    m_recording = false;
    AddDeleteMap<ADDS, DELETES>::reset();
  }

  void start_recording() { m_recording = true; }

  void stop_recording() { m_recording = false; }

  void revert() {
    for (const auto& iter : r_adds) {
      if (iter.second.empty()) {
        // didn't exist previously
        auto find = AddDeleteMap<ADDS, DELETES>::m_adds->find(iter.first);
        if (find != AddDeleteMap<ADDS, DELETES>::m_adds->end()) {
          AddDeleteMap<ADDS, DELETES>::m_adds->erase(find);
        }
      } else {
        // exist previously
        AddDeleteMap<ADDS, DELETES>::insert(iter.first, &iter.second);
      }
    }

    for (const auto& iter : r_deletes) {
      // deleted previously
      AddDeleteMap<ADDS, DELETES>::kill(iter);
    }

    // didn't exist but deleted
    for (const auto& iter : r_newdeletes) {
      auto find = AddDeleteMap<ADDS, DELETES>::m_deletes->find(iter);
      if (find != AddDeleteMap<ADDS, DELETES>::m_deletes->end()) {
        AddDeleteMap<ADDS, DELETES>::m_deletes->erase(find);
      }
    }
  }

  void reset_recordings() {
    r_adds.clear();
    r_deletes.clear();
    r_newdeletes.clear();
  }

 private:
  ADDS r_adds;        // used to be existing, newly added if if value is null
  DELETES r_deletes;  // used to be deleted
  DELETES r_newdeletes;
  bool m_recording = false;
};

struct LevelDBMap : private MapBase {
  LevelDBMap(std::shared_ptr<LevelDB> db) : m_db(std::move(db)) {}
  FINDRESULT exists(dev::h256 const& _h) const {
    return m_db->Exists(_h) ? FOUND : NOTFOUND;
  }
  std::string lookup(dev::h256 const& _h) const { return m_db->Lookup(_h); }
  void insert([[gnu::unused]] dev::h256 const& _h,
              [[gnu::unused]] dev::bytesConstRef _v) {
    // do nothing
  }
  bool kill([[gnu::unused]] dev::h256 const& _h) {
    // do nothing
    return true;
  }

 private:
  std::shared_ptr<LevelDB> m_db;
};

template <class... T>
struct OverlayMap {};

template <class Head, class... Tail>
struct OverlayMap<Head, Tail...> {
  OverlayMap(Head head, Tail... tail) : head(head), tail(tail...) {}

  bool exists(dev::h256 const& _h) const {
    LOG_MARKER();
    auto ret = head->exists(_h);
    if (ret == FOUND) {
      return true;
    } else if (ret == DELETED) {
      // if DELETED, stop recursing
      return false;
    } else if (!sizeof...(Tail)) {
      return false;
    }
    // NOTFOUND AND HAVE TAILS
    return tail.exists(_h);
  }

  std::string lookup(dev::h256 const& _h) const {
    if (head->exists(_h) == DELETED) {
      // if DELETED, stop recursing
      return "";
    }

    std::string ret = head->lookup(_h);
    if (!ret.empty()) {
      return ret;
    } else if (!sizeof...(Tail)) {
      return "";
    }

    return tail.lookup(_h);
  }

  void insert(dev::h256 const& _h, dev::bytesConstRef _v) {
    head->insert(_h, _v);
  }

  bool kill(dev::h256 const& _h) {
    head->kill(_h);

    return true;
  }

 private:
  Head head;
  OverlayMap<Tail...> tail;
};

template <>
struct OverlayMap<> {
  bool exists([[gnu::unused]] dev::h256 const& _h) const {
    LOG_MARKER();
    return false;
  }

  std::string lookup([[gnu::unused]] dev::h256 const& _h) const { return ""; }

  void insert([[gnu::unused]] dev::h256 const& _h,
              [[gnu::unused]] dev::bytesConstRef _v) {}

  bool kill([[gnu::unused]] dev::h256 const& _h) { return false; }
};

}  // namespace Contract

#endif  // ZILLIQA_SRC_LIBPERSISTENCE_CONTRACTSTORAGEOLDDATA_H_