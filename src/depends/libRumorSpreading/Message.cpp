#include <Message.h>
#include "Message.h"

namespace RRS {

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

std::ostream& operator<<(std::ostream& os, const Message& message)
{
    os << "[ type: " << message.m_type << " rumorId: " << message.m_rumorId << " age: "
       << message.m_round << "]";
    return os;
}

} // project namespace