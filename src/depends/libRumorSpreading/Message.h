#ifndef RANDOMIZEDRUMORSPREADING_MESSAGE_H
#define RANDOMIZEDRUMORSPREADING_MESSAGE_H

#include <vector>
#include <memory>
#include <ostream>

namespace RRS {

class Message {
  public:
    // ENUMS
    enum Type {
        UNDEFINED,
        PUSH,
        PULL,
    };

  private:
    // MEMBERS
    Type m_type;
    int m_rumorId;
    int m_round;

  public:
    // CONSTRUCTORS
    Message();

    Message(Type type,
            int rumorId,
            int round);

    // OPERATORS
    bool operator==(const Message& other) const;

    bool operator!=(const Message& other) const;

    friend std::ostream& operator<<(std::ostream& os, const Message& message);

    // CONST METHODS
    Type type() const;

    int rumorId() const;

    int age() const;
};

} // project namespace

#endif //RANDOMIZEDRUMORSPREADING_MESSAGE_H
