#include <Message.h>
#include "Message.h"

#define LITERAL(s) #s

namespace RRS {

// STATIC MEMBERS
std::map<Message::Type, std::string> Message::s_enumKeyToString = {
    {Type::UNDEFINED, LITERAL(UNDEFINED)},
    {Type::PUSH,      LITERAL(PUSH)},
    {Type::PULL,      LITERAL(PULL)},
};

// CONSTRUCTORS
Message::Message()
{
}

Message::Message(Message::Type type,
                 int rumorId,
                 int round)
: m_type(type)
, m_rumorId(rumorId)
, m_round(round)
{
}

// OPERATORS
bool Message::operator==(const Message& other) const
{
    return m_type == other.m_type &&
           m_rumorId == other.m_rumorId &&
           m_round == other.m_round;
}

bool Message::operator!=(const Message& other) const
{
    return !(*this == other);
}

// CONST METHODS
Message::Type Message::type() const
{
    return m_type;
}

int Message::rumorId() const
{
    return m_rumorId;
}

int Message::age() const
{
    return m_round;
}

// FREE OPERATORS
std::ostream& operator<<(std::ostream& os, const Message& message)
{
    os << "[ type: " << Message::s_enumKeyToString[message.m_type]
       << " rumorId: " << message.m_rumorId
       << " age: " << message.m_round
       << "]";
    return os;
}

} // project namespace