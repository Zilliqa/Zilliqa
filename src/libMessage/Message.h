#include "Message.pb.h"

class Message
{
};

enum TYPE
{
    TYPE_0 = 0; TYPE_1 = 1; TYPE_2 = 2;
}

message Nested { required uint32 m_uint32 = 1; }

message Test
{
    // Primitives
    required uint32 m_uint32 = 1;
    required uint64 m_uint64 = 2;
    required bool m_bool = 3;

    // Byte array
    required bytes m_bytes = 4;

    // Repeated, ordered type
    repeated bool m_bitmap = 5 [packed = true];

    // Enumeration
    required TYPE m_type = 6;

    // Nested
    required Nested m_nested = 7;

    // Map -> requires newer version of protobuf-compiler than what apt-get install provides
    //required map<uint32,Nested> m_map = 8;

    // Backward-compatible map type
    message MapEntry
    {
        optional uint32 m_key = 1;
        optional Nested m_val = 2;
    }
}
