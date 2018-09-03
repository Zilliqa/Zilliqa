#ifndef RANDOMIZEDRUMORSPREADING_MEMBERID_H
#define RANDOMIZEDRUMORSPREADING_MEMBERID_H

#include <atomic>

namespace RRS {

class MemberID {
  private:
    // STATIC MEMBERS
    static std::atomic<int> d_id;

  public:
    // STATIC METHODS
    static int next()
    {
        return ++d_id;
    }
};

} // project namespace

#endif //RANDOMIZEDRUMORSPREADING_MEMBERID_H
